#include "LogDialog.h"

#include <windows.h>

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <string>
#include <vector>

#include "Clipboard.h"
#include "Dialogs.h"
#include "Log.h"
#include "resource.h"

namespace kurva::logdialog {

namespace {

using namespace std::chrono_literals;

constexpr UINT_PTR kRefreshTimer = 1;
constexpr UINT_PTR kCopiedTimer = 2;
constexpr UINT kRefreshIntervalMs = 300;
constexpr UINT kCopiedFlashMs = 1500;

struct State {
    HFONT font = nullptr;
    unsigned long long seen = 0;  // log::Recent::total at the last refresh.
    // The template's spacing in pixels, for the layout after a resize.
    int margin = 0;
    int gap = 0;
    SIZE button{};
    int hintHeight = 0;
    POINT minimumSize{};
};

std::wstring joined(const std::vector<std::wstring>& lines, size_t first) {
    std::wstring text;
    for (size_t index = first; index < lines.size(); ++index) {
        if (index > first) {
            text += L"\r\n";
        }
        text += lines[index];
    }
    return text;
}

void moveCaretToEnd(HWND edit) {
    const int length = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    SendMessageW(edit, EM_SCROLLCARET, 0, 0);
}

// Shows the lines logged since the last refresh.
void refresh(HWND dialog, State& state) {
    const log::Recent recent = log::recent();
    if (recent.total == state.seen) {
        return;
    }
    const HWND edit = GetDlgItem(dialog, IDC_LOG_TEXT);
    const unsigned long long fresh = recent.total - state.seen;  // total never decreases.
    state.seen = recent.total;
    if (fresh <= recent.lines.size()) {
        // Append just the new lines; a read-only edit accepts EM_REPLACESEL.
        std::wstring text = joined(recent.lines, recent.lines.size() - static_cast<size_t>(fresh));
        if (GetWindowTextLengthW(edit) > 0) {
            text.insert(0, L"\r\n");
        }
        moveCaretToEnd(edit);
        SendMessageW(edit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    } else {
        SetWindowTextW(edit, joined(recent.lines, 0).c_str());
    }
    moveCaretToEnd(edit);
}

void copyAll(HWND dialog) {
    const HWND edit = GetDlgItem(dialog, IDC_LOG_TEXT);
    const int length = GetWindowTextLengthW(edit);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(edit, text.data(), length + 1);
    text.resize(static_cast<size_t>(std::max(copied, 0)));
    {
        const clipboard::Lock lock(dialog, 500ms);
        if (!lock.isOpen()) {
            log::info(L"log window: cannot open the clipboard");
            return;
        }
        EmptyClipboard();
        if (const HGLOBAL handle = clipboard::makeTextGlobal(text, CF_UNICODETEXT)) {
            if (!SetClipboardData(CF_UNICODETEXT, handle)) {
                GlobalFree(handle);
            }
        }
    }
    SetDlgItemTextW(dialog, IDC_LOG_COPY, L"Copied");
    SetTimer(dialog, kCopiedTimer, kCopiedFlashMs, nullptr);
}

void measure(HWND dialog, State& state) {
    RECT units{0, 0, 8, 14};  // Margin and button height, in dialog units.
    MapDialogRect(dialog, &units);
    state.margin = units.right;
    state.button.cy = units.bottom;
    RECT more{0, 0, 50, 18};  // Button width and hint height.
    MapDialogRect(dialog, &more);
    state.button.cx = more.right;
    state.hintHeight = more.bottom;
    RECT gap{0, 0, 4, 4};
    MapDialogRect(dialog, &gap);
    state.gap = gap.right;
    RECT window{};
    GetWindowRect(dialog, &window);
    state.minimumSize = POINT{window.right - window.left, window.bottom - window.top};
}

// The buttons sit bottom right, the hint bottom left, the log fills the rest.
void layout(HWND dialog, const State& state) {
    RECT client{};
    GetClientRect(dialog, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int rowTop = height - state.margin - state.button.cy;
    int right = width - state.margin;
    for (const int id : {IDCANCEL, IDC_LOG_CLEAR, IDC_LOG_COPY}) {
        right -= state.button.cx;
        SetWindowPos(GetDlgItem(dialog, id), nullptr, right, rowTop, state.button.cx, state.button.cy,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        right -= state.gap;
    }
    const int hintTop = height - state.margin - state.hintHeight;
    SetWindowPos(GetDlgItem(dialog, IDC_LOG_HINT), nullptr, state.margin, hintTop,
                 std::max(0, right - state.margin), state.hintHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    const int logBottom = std::min(rowTop, hintTop) - state.gap;
    SetWindowPos(GetDlgItem(dialog, IDC_LOG_TEXT), nullptr, state.margin, state.margin,
                 std::max(0, width - 2 * state.margin), std::max(0, logBottom - state.margin),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        state = reinterpret_cast<State*>(lParam);
        measure(dialog, *state);
        const HWND edit = GetDlgItem(dialog, IDC_LOG_TEXT);
        state->font = dialogs::deriveFont(dialog, 100, false, L"Consolas");
        if (state->font) {
            SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
        }
        SendMessageW(edit, EM_SETLIMITTEXT, 0, 0);  // 0: as much as an edit control can hold.
        refresh(dialog, *state);
        SetTimer(dialog, kRefreshTimer, kRefreshIntervalMs, nullptr);
        dialogs::centerOnMouseMonitor(dialog);
        dialogs::registerOpen(dialog);
        ShowWindow(dialog, SW_SHOW);
        SetForegroundWindow(dialog);
        return TRUE;
    }

    case WM_SIZE:
        if (state && wParam != SIZE_MINIMIZED) {
            layout(dialog, *state);
        }
        return TRUE;

    case WM_GETMINMAXINFO:
        if (state) {
            reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = state->minimumSize;
        }
        return TRUE;

    case WM_TIMER:
        if (wParam == kRefreshTimer && state) {
            refresh(dialog, *state);
        } else if (wParam == kCopiedTimer) {
            KillTimer(dialog, kCopiedTimer);
            SetDlgItemTextW(dialog, IDC_LOG_COPY, L"Copy");
        }
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_LOG_COPY:
            copyAll(dialog);
            return TRUE;
        case IDC_LOG_CLEAR:
            log::clear();
            SetDlgItemTextW(dialog, IDC_LOG_TEXT, L"");
            if (state) {
                state->seen = log::recent().total;
            }
            return TRUE;
        case IDOK:
        case IDCANCEL:
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        default:
            return FALSE;
        }

    case WM_DESTROY:
        KillTimer(dialog, kRefreshTimer);
        KillTimer(dialog, kCopiedTimer);
        dialogs::registerClosed(dialog);
        if (state && state->font) {
            DeleteObject(state->font);
            state->font = nullptr;
        }
        return FALSE;

    default:
        return FALSE;
    }
}

}  // namespace

void show(HINSTANCE instance, HWND owner) {
    State state;
    if (DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_LOG), owner, dialogProc, reinterpret_cast<LPARAM>(&state)) == -1) {
        log::info(L"log window: cannot be shown (error {})", GetLastError());
    }
}

}  // namespace kurva::logdialog
