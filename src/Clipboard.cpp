#include "Clipboard.h"

#include <cstring>
#include <limits>
#include <utility>

#include "Log.h"
#include "MessagePump.h"

namespace kurva::clipboard {

namespace {

using namespace std::chrono_literals;

// Anything bigger is left out of the snapshot (and therefore lost after a conversion)
// rather than duplicated. Screenshots of a 4K display are ~33 MB, so this is generous.
constexpr size_t kMaxFormatBytes = 256u * 1024u * 1024u;

// Formats that must not (or cannot) be duplicated with GlobalAlloc/memcpy.
bool isRestorable(UINT format) {
    switch (format) {
    case CF_BITMAP:          // GDI object; Windows re-synthesizes it from CF_DIB / CF_DIBV5.
    case CF_METAFILEPICT:    // Wraps a GDI metafile; re-synthesized from CF_ENHMETAFILE.
    case CF_PALETTE:         // GDI object.
    case CF_OWNERDISPLAY:    // Only meaningful while the original owner is alive.
    case CF_DSPTEXT:
    case CF_DSPBITMAP:
    case CF_DSPMETAFILEPICT:
    case CF_DSPENHMETAFILE:
        return false;
    default:
        break;
    }
    if (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST) {
        return false;  // Owner-private data with owner-defined lifetime.
    }
    if (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST) {
        return false;  // GDI objects.
    }
    return true;
}

struct HistoryFormats {
    UINT canIncludeInHistory;
    UINT canUploadToCloud;
};

const HistoryFormats& historyFormats() {
    static const HistoryFormats formats{
        RegisterClipboardFormatW(L"CanIncludeInClipboardHistory"),
        RegisterClipboardFormatW(L"CanUploadToCloudClipboard"),
    };
    return formats;
}

}  // namespace

Lock::Lock(HWND owner, std::chrono::milliseconds timeout) {
    open_ = MessagePump::waitUntil([owner] { return OpenClipboard(owner) != FALSE; }, timeout, 2ms);
}

Lock::~Lock() {
    if (open_) {
        CloseClipboard();
    }
}

HGLOBAL makeGlobal(const void* bytes, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    const HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!handle) {
        return nullptr;
    }
    void* destination = GlobalLock(handle);
    if (!destination) {
        GlobalFree(handle);
        return nullptr;
    }
    std::memcpy(destination, bytes, size);
    GlobalUnlock(handle);
    return handle;
}

HGLOBAL makeTextGlobal(std::wstring_view text, UINT format) {
    if (format == CF_UNICODETEXT) {
        const std::wstring terminated(text);
        return makeGlobal(terminated.c_str(), (terminated.size() + 1) * sizeof(wchar_t));
    }
    if (format != CF_TEXT && format != CF_OEMTEXT) {
        return nullptr;
    }
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return nullptr;
    }
    const UINT codePage = (format == CF_OEMTEXT) ? CP_OEMCP : CP_ACP;
    const int length = static_cast<int>(text.size());
    const int needed = length == 0 ? 0
        : WideCharToMultiByte(codePage, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (needed < 0) {
        return nullptr;
    }
    std::string narrow(static_cast<size_t>(needed) + 1, '\0');
    if (needed > 0) {
        WideCharToMultiByte(codePage, 0, text.data(), length, narrow.data(), needed, nullptr, nullptr);
    }
    return makeGlobal(narrow.data(), narrow.size());
}

void addHistoryOptOut() {
    const HistoryFormats& formats = historyFormats();
    for (const UINT format : {formats.canIncludeInHistory, formats.canUploadToCloud}) {
        if (format == 0) {
            continue;
        }
        const DWORD no = 0;
        if (const HGLOBAL handle = makeGlobal(&no, sizeof(no))) {
            if (!SetClipboardData(format, handle)) {
                GlobalFree(handle);
            }
        }
    }
}

std::optional<std::wstring> readUnicodeText(HWND owner, std::chrono::milliseconds timeout) {
    const Lock lock(owner, timeout);
    if (!lock.isOpen()) {
        log::info(L"clipboard: cannot open it for reading");
        return std::nullopt;
    }
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return std::nullopt;
    }
    const HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        return std::nullopt;
    }
    const auto* chars = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!chars) {
        return std::nullopt;
    }
    const size_t maxChars = GlobalSize(handle) / sizeof(wchar_t);
    size_t length = 0;
    while (length < maxChars && chars[length] != L'\0') {
        ++length;
    }
    std::wstring text(chars, length);
    GlobalUnlock(handle);
    return text;
}

Snapshot::~Snapshot() {
    release();
}

Snapshot::Snapshot(Snapshot&& other) noexcept
    : entries_(std::move(other.entries_)), captured_(other.captured_) {
    other.entries_.clear();
    other.captured_ = false;
}

Snapshot& Snapshot::operator=(Snapshot&& other) noexcept {
    if (this != &other) {
        release();
        entries_ = std::move(other.entries_);
        captured_ = other.captured_;
        other.entries_.clear();
        other.captured_ = false;
    }
    return *this;
}

bool Snapshot::capture(HWND owner, std::chrono::milliseconds timeout) {
    release();

    const Lock lock(owner, timeout);
    if (!lock.isOpen()) {
        log::info(L"clipboard: cannot open it to take a snapshot");
        return false;
    }

    size_t skipped = 0;
    for (UINT format = EnumClipboardFormats(0); format != 0; format = EnumClipboardFormats(format)) {
        if (!isRestorable(format)) {
            ++skipped;
            continue;
        }
        // This asks the current owner to render the format if it uses delayed rendering.
        const HANDLE source = GetClipboardData(format);
        if (!source) {
            ++skipped;
            continue;
        }

        Entry entry;
        entry.format = format;
        entry.isEnhMetafile = (format == CF_ENHMETAFILE);
        if (entry.isEnhMetafile) {
            entry.handle = CopyEnhMetaFileW(static_cast<HENHMETAFILE>(source), nullptr);
        } else {
            const size_t size = GlobalSize(source);
            if (size == 0 || size > kMaxFormatBytes) {
                ++skipped;
                continue;
            }
            if (const void* data = GlobalLock(source)) {
                entry.handle = makeGlobal(data, size);
                GlobalUnlock(source);
            }
        }

        if (entry.handle) {
            entries_.push_back(entry);
        } else {
            ++skipped;
        }
    }

    captured_ = true;
    log::info(L"clipboard: snapshot of {} format(s) taken, {} skipped", entries_.size(), skipped);
    return true;
}

bool Snapshot::restore(HWND owner, HWND requiredOwner, std::chrono::milliseconds timeout) {
    const Lock lock(owner, timeout);
    if (!lock.isOpen()) {
        log::info(L"clipboard: cannot open it to restore the snapshot");
        return false;
    }
    if (requiredOwner && GetClipboardOwner() != requiredOwner) {
        log::info(L"clipboard: owned by somebody else now, leaving it alone");
        return false;
    }
    if (!EmptyClipboard()) {
        log::info(L"clipboard: EmptyClipboard failed (error {})", GetLastError());
        return false;
    }

    bool complete = true;
    for (Entry& entry : entries_) {
        if (!entry.handle) {
            continue;
        }
        if (SetClipboardData(entry.format, entry.handle)) {
            entry.handle = nullptr;  // Owned by the system now.
        } else {
            complete = false;
            freeHandle(entry);
        }
    }
    if (!entries_.empty()) {
        // The original copy is already in the history; putting it back is not a new copy.
        addHistoryOptOut();
    }

    log::info(L"clipboard: snapshot restored{}", complete ? L"" : L" (some formats failed)");
    return true;
}

void Snapshot::freeHandle(Entry& entry) noexcept {
    if (!entry.handle) {
        return;
    }
    if (entry.isEnhMetafile) {
        DeleteEnhMetaFile(static_cast<HENHMETAFILE>(entry.handle));
    } else {
        GlobalFree(entry.handle);
    }
    entry.handle = nullptr;
}

void Snapshot::release() noexcept {
    for (Entry& entry : entries_) {
        freeHandle(entry);
    }
    entries_.clear();
    captured_ = false;
}

}  // namespace kurva::clipboard
