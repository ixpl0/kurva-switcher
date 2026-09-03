#include "TextSwitcher.h"

#include <utility>
#include <vector>

#include "Input.h"
#include "Log.h"
#include "MessagePump.h"

namespace kurva {

namespace {

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

constexpr wchar_t kClassName[] = L"KurvaSwitcher.Clipboard";

// Ctrl+C: how long we wait for the application to put something on the clipboard.
constexpr auto kCopyTimeout = 1000ms;
// Ctrl+C: some applications write the clipboard in several steps; wait until it is quiet.
constexpr auto kCopySettle = 30ms;
constexpr auto kCopySettleLimit = 250ms;
// Ctrl+V: longest time the converted text stays on the clipboard if nobody reads it.
constexpr auto kPasteTimeout = 2000ms;
// Ctrl+V: quiet time after the target's last clipboard read before we restore the original.
// Some applications read the clipboard once to inspect it and once more to paste.
constexpr auto kPasteSettle = 300ms;
// Longest time to wait for another application to release the clipboard.
constexpr auto kClipboardOpenTimeout = 500ms;

struct BusyGuard {
    bool& flag;
    explicit BusyGuard(bool& target) : flag(target) { flag = true; }
    ~BusyGuard() { flag = false; }
};

struct QuitReposter {
    ~QuitReposter() { MessagePump::repostQuitIfPending(); }
};

DWORD processIdOf(HWND hwnd) {
    DWORD processId = 0;
    if (hwnd) {
        GetWindowThreadProcessId(hwnd, &processId);
    }
    return processId;
}

BOOL CALLBACK collectChildProcessIds(HWND child, LPARAM param) {
    auto* processIds = reinterpret_cast<std::set<DWORD>*>(param);
    if (const DWORD processId = processIdOf(child)) {
        processIds->insert(processId);
    }
    return TRUE;
}

long long millisecondsSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

}  // namespace

TextSwitcher::TextSwitcher(HINSTANCE instance) : instance_(instance) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &TextSwitcher::windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    classRegistered_ = RegisterClassExW(&windowClass) != 0;
    if (!classRegistered_) {
        log::info(L"switcher: RegisterClassEx failed (error {})", GetLastError());
        return;
    }

    // A message-only window: it owns the clipboard while converted text is published and
    // answers WM_RENDERFORMAT, but never shows up anywhere.
    const HWND created = CreateWindowExW(0, kClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
    if (!created) {
        window_ = nullptr;
        log::info(L"switcher: CreateWindowEx failed (error {})", GetLastError());
        return;
    }
    window_ = created;
    // Wakes our waits up as soon as the clipboard changes; the decision itself is always
    // taken from the clipboard sequence number, never from this notification.
    AddClipboardFormatListener(window_);
    log::info(L"switcher: ready (UI Automation {})", selection_.available() ? L"available" : L"unavailable");
}

TextSwitcher::~TextSwitcher() {
    if (window_) {
        RemoveClipboardFormatListener(window_);
        DestroyWindow(window_);
        window_ = nullptr;
    }
    if (classRegistered_) {
        UnregisterClassW(kClassName, instance_);
    }
}

LRESULT CALLBACK TextSwitcher::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* self = static_cast<TextSwitcher*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->window_ = hwnd;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    auto* self = reinterpret_cast<TextSwitcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handleMessage(message, wParam, lParam);
}

LRESULT TextSwitcher::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_RENDERFORMAT:
        onRenderFormat(static_cast<UINT>(wParam));
        return 0;
    case WM_RENDERALLFORMATS:
        onRenderAllFormats();
        return 0;
    case WM_DESTROYCLIPBOARD:
        onDestroyClipboard();
        return 0;
    case WM_CLIPBOARDUPDATE:
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

// Windows sends this from inside the reader's GetClipboardData call, so the reader is
// blocked until we return and the clipboard is still open by its window.
void TextSwitcher::onRenderFormat(UINT format) {
    if (!ownsClipboard_) {
        log::info(L"paste: render request for format {} arrived after we stopped publishing", format);
        return;
    }

    if (const HGLOBAL handle = clipboard::makeTextGlobal(published_, format)) {
        if (!SetClipboardData(format, handle)) {
            GlobalFree(handle);
            log::info(L"paste: SetClipboardData({}) failed while rendering (error {})", format, GetLastError());
        }
    } else {
        log::info(L"paste: cannot render format {}", format);
    }

    const HWND reader = GetOpenClipboardWindow();
    const DWORD readerProcessId = processIdOf(reader);
    const long long sinceLastRelevant = millisecondsSince(lastRelevantRender_);
    if (readerProcessId != 0 && targetProcessIds_.contains(readerProcessId)) {
        ++targetRenders_;
        lastRelevantRender_ = Clock::now();
        log::info(L"paste: target process {} read format {}", readerProcessId, format);
    } else if (readerProcessId == 0) {
        // Opened with OpenClipboard(NULL): could be anybody, including the target.
        ++unknownRenders_;
        lastRelevantRender_ = Clock::now();
        log::info(L"paste: unidentified reader read format {}", format);
    } else {
        // Clipboard managers, remote desktop, sync services: not the paste we wait for.
        ++foreignRenders_;
        log::info(L"paste: process {} read format {} (not the target; {} ms since the last relevant read)",
                  readerProcessId, format, sinceLastRelevant);
    }
}

// Our window is being destroyed while it still owns delayed-rendered data.
void TextSwitcher::onRenderAllFormats() {
    if (!ownsClipboard_ || !OpenClipboard(window_)) {
        return;
    }
    if (GetClipboardOwner() == window_) {
        if (const HGLOBAL handle = clipboard::makeTextGlobal(published_, CF_UNICODETEXT)) {
            if (!SetClipboardData(CF_UNICODETEXT, handle)) {
                GlobalFree(handle);
            }
        }
    }
    CloseClipboard();
}

void TextSwitcher::onDestroyClipboard() {
    if (!ownsClipboard_) {
        return;
    }
    const HWND opener = GetOpenClipboardWindow();
    if (opener == window_) {
        return;  // Our own EmptyClipboard while restoring the snapshot.
    }
    ownsClipboard_ = false;
    clipboardTaken_ = true;
    log::info(L"paste: clipboard taken over by process {}", processIdOf(opener));
}

TextSwitcher::Target TextSwitcher::captureTarget(DWORD focusedProcessId) const {
    Target target;
    if (focusedProcessId != 0) {
        target.processIds.insert(focusedProcessId);
    }
    target.foreground = GetForegroundWindow();
    if (target.foreground) {
        DWORD foregroundProcessId = 0;
        const DWORD threadId = GetWindowThreadProcessId(target.foreground, &foregroundProcessId);
        if (foregroundProcessId != 0) {
            target.processIds.insert(foregroundProcessId);
        }
        GUITHREADINFO threadInfo{};
        threadInfo.cbSize = sizeof(threadInfo);
        if (GetGUIThreadInfo(threadId, &threadInfo) && threadInfo.hwndFocus) {
            target.focus = threadInfo.hwndFocus;
            if (const DWORD focusProcessId = processIdOf(threadInfo.hwndFocus)) {
                target.processIds.insert(focusProcessId);
            }
        }
        // Store apps live in a child window of ApplicationFrameHost's frame; collect them too.
        EnumChildWindows(target.foreground, &collectChildProcessIds, reinterpret_cast<LPARAM>(&target.processIds));
        if (!target.focus) {
            target.focus = FindWindowExW(target.foreground, nullptr, L"Windows.UI.Core.CoreWindow", nullptr);
        }
    }
    target.processIds.erase(GetCurrentProcessId());
    return target;
}

void TextSwitcher::run() {
    if (!window_) {
        return;
    }
    if (busy_) {
        log::info(L"hotkey ignored: a conversion is still in progress");
        return;
    }
    const BusyGuard busy(busy_);
    const QuitReposter quitReposter;
    const auto started = Clock::now();
    log::info(L"---- hotkey ----");

    published_.clear();
    ownsClipboard_ = false;
    clipboardTaken_ = false;
    targetRenders_ = unknownRenders_ = foreignRenders_ = 0;
    backup_ = clipboard::Snapshot();

    // 1. Find out what is selected, preferably without touching the clipboard.
    Selection selection = selection_.read();
    const Target target = captureTarget(selection.processId);
    targetProcessIds_ = target.processIds;

    bool modifiersReleased = false;
    const auto releaseModifiersOnce = [&modifiersReleased] {
        if (!modifiersReleased) {
            modifiersReleased = true;
            if (const int released = input::releaseModifiers()) {
                log::info(L"input: released {} modifier key(s)", released);
            }
        }
    };

    std::wstring text;
    HWND ownerAfterCopy = nullptr;  // Set when Ctrl+C replaced the user's clipboard content.
    if (selection.status == Selection::Status::Text) {
        text = std::move(selection.text);
    } else if (selection.status == Selection::Status::Empty && selection.emptyIsTrusted) {
        log::info(L"nothing is selected; done in {} ms", millisecondsSince(started));
        return;
    } else {
        // 2. Fall back to Ctrl+C. Back the clipboard up first: the copy will overwrite it.
        if (!backup_.capture(window_, kClipboardOpenTimeout)) {
            log::info(L"giving up: the clipboard is not available");
            return;
        }
        releaseModifiersOnce();
        CopyResult copied = copySelectionWithCtrlC();
        switch (copied.outcome) {
        case CopyResult::Outcome::ClipboardUnavailable:
            log::info(L"giving up: the clipboard is not available");
            return;
        case CopyResult::Outcome::NothingCopied:
            log::info(L"Ctrl+C changed nothing: nothing is selected; restoring the clipboard");
            restoreClipboard(window_);
            return;
        case CopyResult::Outcome::Copied:
            break;
        }
        ownerAfterCopy = copied.owner;
        if (copied.text.empty()) {
            log::info(L"Ctrl+C put no text on the clipboard; restoring it");
            restoreClipboard(ownerAfterCopy);
            return;
        }
        text = std::move(copied.text);
        log::info(L"selection via Ctrl+C: {} character(s)", text.size());
    }

    // 3. Convert.
    const Conversion conversion = converter_.convert(text);
    if (!conversion.changed) {
        log::info(L"the selection has nothing to convert");
        if (backup_.captured()) {
            restoreClipboard(ownerAfterCopy);
        }
        return;
    }

    // 4. Publish the converted text and paste it.
    if (!backup_.captured() && !backup_.capture(window_, kClipboardOpenTimeout)) {
        log::info(L"giving up: the clipboard is not available");
        return;
    }
    if (!publish(conversion.text)) {
        if (ownerAfterCopy) {
            restoreClipboard(ownerAfterCopy);
        }
        return;
    }
    releaseModifiersOnce();
    if (!input::sendCtrlChord('V')) {
        log::info(L"cannot send Ctrl+V");
    }
    waitForPaste();

    // 5. Give the user's clipboard back.
    restoreClipboard(window_);

    // 6. Continue typing in the layout the text now belongs to.
    if (switchLayout_ && conversion.lastTarget != Layout::Unknown) {
        switchKeyboardLayout(target, conversion.lastTarget);
    }
    log::info(L"done in {} ms", millisecondsSince(started));
}

TextSwitcher::CopyResult TextSwitcher::copySelectionWithCtrlC() {
    CopyResult result;

    // Start from an empty clipboard that we own. Then the only way the sequence number can
    // change is a real write by the application - even when it copies exactly the text that
    // was on the clipboard before - and "nothing copied" is unambiguous.
    {
        const clipboard::Lock lock(window_, kClipboardOpenTimeout);
        if (!lock.isOpen()) {
            log::info(L"copy: cannot open the clipboard");
            return result;
        }
        if (!EmptyClipboard()) {
            log::info(L"copy: EmptyClipboard failed (error {})", GetLastError());
            return result;
        }
    }
    result.outcome = CopyResult::Outcome::NothingCopied;

    const DWORD sequenceBefore = GetClipboardSequenceNumber();
    const auto sent = Clock::now();
    if (!input::sendCtrlChord('C')) {
        log::info(L"copy: cannot send Ctrl+C");
        return result;
    }

    const auto changed = [sequenceBefore] { return GetClipboardSequenceNumber() != sequenceBefore; };
    if (!MessagePump::waitUntil(changed, kCopyTimeout)) {
        return result;
    }
    const long long reactionTime = millisecondsSince(sent);

    // Let applications that write several formats in separate steps finish.
    DWORD lastSequence = GetClipboardSequenceNumber();
    auto quietSince = Clock::now();
    const auto settleDeadline = Clock::now() + kCopySettleLimit;
    while (Clock::now() < settleDeadline) {
        MessagePump::pumpFor(10ms);
        const DWORD sequence = GetClipboardSequenceNumber();
        if (sequence != lastSequence) {
            lastSequence = sequence;
            quietSince = Clock::now();
        } else if (Clock::now() - quietSince >= kCopySettle) {
            break;
        }
    }
    log::info(L"copy: the clipboard changed {} ms after Ctrl+C", reactionTime);

    result.outcome = CopyResult::Outcome::Copied;
    result.owner = GetClipboardOwner();
    result.text = clipboard::readUnicodeText(window_, kClipboardOpenTimeout).value_or(std::wstring());
    return result;
}

bool TextSwitcher::publish(const std::wstring& text) {
    const clipboard::Lock lock(window_, kClipboardOpenTimeout);
    if (!lock.isOpen()) {
        log::info(L"paste: cannot open the clipboard to publish the converted text");
        return false;
    }
    if (!EmptyClipboard()) {
        log::info(L"paste: EmptyClipboard failed (error {})", GetLastError());
        return false;
    }

    published_ = text;
    ownsClipboard_ = true;
    clipboardTaken_ = false;
    targetRenders_ = unknownRenders_ = foreignRenders_ = 0;
    lastRelevantRender_ = Clock::now();

    // Delayed rendering: the data is produced in onRenderFormat when somebody reads it.
    // SetClipboardData returns NULL for a NULL handle whether it succeeded or not, so the
    // format's availability is checked instead.
    SetClipboardData(CF_UNICODETEXT, nullptr);
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        log::info(L"paste: SetClipboardData(CF_UNICODETEXT, delayed) failed (error {})", GetLastError());
        published_.clear();
        ownsClipboard_ = false;
        return false;
    }
    // The converted text is a transient; keep it out of Win+V and the cloud clipboard.
    clipboard::addHistoryOptOut();
    log::info(L"paste: published {} character(s) with delayed rendering", text.size());
    return true;
}

void TextSwitcher::waitForPaste() {
    const auto started = Clock::now();
    const auto done = [this] {
        if (clipboardTaken_) {
            return true;
        }
        return (targetRenders_ + unknownRenders_) > 0 && Clock::now() - lastRelevantRender_ >= kPasteSettle;
    };
    MessagePump::waitUntil(done, kPasteTimeout, 10ms);
    log::info(L"paste: waited {} ms; reads by the target {}, unidentified {}, others {}{}",
              millisecondsSince(started), targetRenders_, unknownRenders_, foreignRenders_,
              clipboardTaken_ ? L"; clipboard taken over" : L"");
}

void TextSwitcher::restoreClipboard(HWND requiredOwner) {
    if (clipboardTaken_) {
        log::info(L"restore: skipped, the clipboard holds somebody else's new content");
        backup_ = clipboard::Snapshot();
        published_.clear();
        ownsClipboard_ = false;
        return;
    }
    if (backup_.restore(window_, requiredOwner, kClipboardOpenTimeout)) {
        backup_ = clipboard::Snapshot();
        published_.clear();
        ownsClipboard_ = false;
    } else if (ownsClipboard_) {
        // Keep serving the converted text; the next conversion replaces it anyway.
        log::info(L"restore: failed, the converted text stays on the clipboard");
    }
}

void TextSwitcher::switchKeyboardLayout(const Target& target, Layout layout) const {
    const WORD wantedLanguage = (layout == Layout::Cyrillic) ? LANG_RUSSIAN : LANG_ENGLISH;
    const HWND window = target.focus ? target.focus : target.foreground;
    if (!window) {
        return;
    }

    const DWORD threadId = GetWindowThreadProcessId(window, nullptr);
    const HKL current = GetKeyboardLayout(threadId);
    const auto languageOf = [](HKL layoutHandle) {
        return PRIMARYLANGID(LOWORD(reinterpret_cast<ULONG_PTR>(layoutHandle)));
    };
    if (languageOf(current) == wantedLanguage) {
        return;
    }

    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count <= 0) {
        return;
    }
    std::vector<HKL> layouts(static_cast<size_t>(count));
    const int fetched = GetKeyboardLayoutList(count, layouts.data());
    for (int index = 0; index < fetched; ++index) {
        const HKL candidate = layouts[static_cast<size_t>(index)];
        if (languageOf(candidate) == wantedLanguage) {
            PostMessageW(window, WM_INPUTLANGCHANGEREQUEST, 0, reinterpret_cast<LPARAM>(candidate));
            log::info(L"layout: asked the target to switch to language {:#x}", wantedLanguage);
            return;
        }
    }
    log::info(L"layout: no installed layout for language {:#x}", wantedLanguage);
}

}  // namespace kurva
