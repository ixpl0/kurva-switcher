#include "HotkeyDialog.h"

#include <windows.h>

#include <commctrl.h>

#include <string>

#include "Log.h"
#include "resource.h"

namespace kurva::hotkeydialog {

namespace {

constexpr wchar_t kTitle[] = L"kurva-switcher";

// Id for the RegisterHotKey probe below. Hotkey ids are per thread; the application's own
// ones belong to its window, so there is no clash.
constexpr int kProbeId = 0x4B55;

HWND openDialog = nullptr;

struct State {
    Hotkey current;
    Hotkey chosen;
};

// Keys nobody types with, so taking them over on their own is fine. Any other key must come
// with Ctrl, Alt or Win, or the user could no longer type that character, or use Enter, Home...
bool isSpareKey(UINT virtualKey) {
    switch (virtualKey) {
    case VK_PAUSE:
    case VK_CANCEL:  // Ctrl+Pause arrives as this key ("Break").
    case VK_SCROLL:
    case VK_SNAPSHOT:
    case VK_APPS:
        return true;
    default:
        return virtualKey >= VK_F1 && virtualKey <= VK_F24;
    }
}

bool takenByAnotherProgram(const Hotkey& hotkey) {
    // A registration on this thread, with no window, is a harmless way to ask Windows.
    if (!RegisterHotKey(nullptr, kProbeId, hotkey.modifiers, hotkey.virtualKey)) {
        return GetLastError() == ERROR_HOTKEY_ALREADY_REGISTERED;
    }
    UnregisterHotKey(nullptr, kProbeId);
    return false;
}

// Why the combination cannot be used, or nullptr when it can.
const wchar_t* problem(const Hotkey& hotkey) {
    if (hotkey.empty()) {
        return L"Press the key combination first.";
    }
    if (!(hotkey.modifiers & (MOD_CONTROL | MOD_ALT | MOD_WIN)) && !isSpareKey(hotkey.virtualKey)) {
        return L"On its own this key is needed for ordinary typing. Add Ctrl or Alt to the combination.";
    }
    if (takenByAnotherProgram(hotkey)) {
        return L"Another program already uses this combination. Choose a different one.";
    }
    return nullptr;
}

// The field is an ordinary edit control that does not type: it shows the name of whatever
// combination is pressed in it instead. The standard hot key control (msctls_hotkey32) would
// be the obvious choice, but it names keys through MapVirtualKey, which has no answer for
// Pause, so the one key this program is known for came out blank in it.

bool isDown(int virtualKey) {
    return (GetKeyState(virtualKey) & 0x8000) != 0;
}

void showHotkey(HWND field, const Hotkey& hotkey) {
    const std::wstring text = hotkey.empty() ? std::wstring() : hotkey.name();
    SetWindowTextW(field, text.c_str());
    const auto end = static_cast<LPARAM>(text.size());
    SendMessageW(field, EM_SETSEL, static_cast<WPARAM>(end), end);  // Caret after the text, nothing selected.
}

// Returns false for the few key presses the field leaves to Windows.
bool captureKey(HWND field, State& state, UINT virtualKey, LPARAM lParam) {
    switch (virtualKey) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_PROCESSKEY:  // IME composition.
    case VK_PACKET:      // Unicode characters injected by SendInput.
        return true;     // A modifier on its own: wait for the key that goes with it.
    default:
        break;
    }
    Hotkey hotkey;
    hotkey.virtualKey = virtualKey;
    hotkey.extendedKey = (HIWORD(lParam) & KF_EXTENDED) != 0;
    if (isDown(VK_CONTROL)) {
        hotkey.modifiers |= MOD_CONTROL;
    }
    if (isDown(VK_SHIFT)) {
        hotkey.modifiers |= MOD_SHIFT;
    }
    if (isDown(VK_MENU)) {
        hotkey.modifiers |= MOD_ALT;
    }
    if (isDown(VK_LWIN) || isDown(VK_RWIN)) {
        hotkey.modifiers |= MOD_WIN;
    }
    if (virtualKey == VK_F4 && hotkey.modifiers == MOD_ALT) {
        return false;  // Still closes the dialog.
    }
    if (virtualKey == VK_BACK && hotkey.modifiers == 0) {
        hotkey = Hotkey{};  // Backspace clears the field, as in the standard control.
    }
    state.chosen = hotkey;
    showHotkey(field, hotkey);
    return true;
}

LRESULT CALLBACK fieldProc(HWND field, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR refData) {
    auto* state = reinterpret_cast<State*>(refData);
    switch (message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (captureKey(field, *state, static_cast<UINT>(wParam), lParam)) {
            return 0;
        }
        break;
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_CONTEXTMENU:
        return 0;  // No typing, no beep for Alt+letter, no Undo/Paste menu.
    case WM_GETDLGCODE:
        // Tab, Enter and Escape stay with the dialog; the text is not to be selected on focus.
        return DefSubclassProc(field, message, wParam, lParam) & ~DLGC_HASSETSEL;
    case WM_NCDESTROY:
        RemoveWindowSubclass(field, fieldProc, 0);
        break;
    default:
        break;
    }
    return DefSubclassProc(field, message, wParam, lParam);
}

// The user has just clicked the tray menu on some monitor; that is where the dialog belongs,
// not necessarily on the primary monitor where DS_CENTER puts it.
void centerOnMouseMonitor(HWND dialog) {
    POINT cursor{};
    GetCursorPos(&cursor);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    RECT window{};
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor) ||
        !GetWindowRect(dialog, &window)) {
        return;
    }
    const RECT& work = monitor.rcWork;
    const LONG x = work.left + ((work.right - work.left) - (window.right - window.left)) / 2;
    const LONG y = work.top + ((work.bottom - work.top) - (window.bottom - window.top)) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        auto* state = reinterpret_cast<State*>(lParam);
        const HWND field = GetDlgItem(dialog, IDC_HOTKEY);
        SetWindowSubclass(field, fieldProc, 0, reinterpret_cast<DWORD_PTR>(state));
        showHotkey(field, state->current);
        centerOnMouseMonitor(dialog);
        openDialog = dialog;
        ShowWindow(dialog, SW_SHOW);
        SetForegroundWindow(dialog);
        return TRUE;  // The dialog manager then focuses the first control: the field.
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            const auto* state = reinterpret_cast<const State*>(GetWindowLongPtrW(dialog, DWLP_USER));
            if (const wchar_t* why = problem(state->chosen)) {
                MessageBoxW(dialog, why, kTitle, MB_ICONINFORMATION);
                return TRUE;
            }
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            return FALSE;
        }

    case WM_DESTROY:
        openDialog = nullptr;
        return FALSE;

    default:
        return FALSE;
    }
}

}  // namespace

std::optional<Hotkey> ask(HINSTANCE instance, HWND owner, const Hotkey& current) {
    State state{.current = current, .chosen = current};
    const INT_PTR result = DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_HOTKEY), owner, dialogProc,
                                           reinterpret_cast<LPARAM>(&state));
    if (result == -1) {
        log::info(L"hotkey dialog: cannot be shown (error {})", GetLastError());
        return std::nullopt;
    }
    if (result != IDOK) {
        return std::nullopt;
    }
    return state.chosen;
}

void focus() {
    if (openDialog) {
        SetForegroundWindow(openDialog);
    }
}

}  // namespace kurva::hotkeydialog
