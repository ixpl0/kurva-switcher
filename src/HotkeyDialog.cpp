#include "HotkeyDialog.h"

#include <windows.h>

#include <commctrl.h>

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

// The hot key control packs the virtual key into the low byte of a WORD and HOTKEYF_* flags
// into the high one; RegisterHotKey wants MOD_* flags, which are numbered differently.
WORD toControl(const Hotkey& hotkey) {
    UINT flags = 0;
    if (hotkey.modifiers & MOD_SHIFT) {
        flags |= HOTKEYF_SHIFT;
    }
    if (hotkey.modifiers & MOD_CONTROL) {
        flags |= HOTKEYF_CONTROL;
    }
    if (hotkey.modifiers & MOD_ALT) {
        flags |= HOTKEYF_ALT;
    }
    if (hotkey.extendedKey) {
        flags |= HOTKEYF_EXT;
    }
    return MAKEWORD(hotkey.virtualKey, flags);
}

Hotkey fromControl(WORD packed) {
    const UINT flags = HIBYTE(packed);
    Hotkey hotkey;
    hotkey.virtualKey = LOBYTE(packed);
    if (flags & HOTKEYF_SHIFT) {
        hotkey.modifiers |= MOD_SHIFT;
    }
    if (flags & HOTKEYF_CONTROL) {
        hotkey.modifiers |= MOD_CONTROL;
    }
    if (flags & HOTKEYF_ALT) {
        hotkey.modifiers |= MOD_ALT;
    }
    hotkey.extendedKey = (flags & HOTKEYF_EXT) != 0;
    return hotkey;
}

// Keys nobody types with, so taking them over on their own is fine. Any other key must come
// with Ctrl or Alt, or the user could no longer type that character, or use Enter, Home...
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
    if (!(hotkey.modifiers & (MOD_CONTROL | MOD_ALT)) && !isSpareKey(hotkey.virtualKey)) {
        return L"On its own this key is needed for ordinary typing. Add Ctrl or Alt to the combination.";
    }
    if (takenByAnotherProgram(hotkey)) {
        return L"Another program already uses this combination. Choose a different one.";
    }
    return nullptr;
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
        const auto* state = reinterpret_cast<const State*>(lParam);
        SendDlgItemMessageW(dialog, IDC_HOTKEY, HKM_SETHOTKEY, toControl(state->current), 0);
        centerOnMouseMonitor(dialog);
        openDialog = dialog;
        ShowWindow(dialog, SW_SHOW);
        SetForegroundWindow(dialog);
        return TRUE;  // The dialog manager then focuses the first control: the hot key field.
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            const WORD packed = static_cast<WORD>(SendDlgItemMessageW(dialog, IDC_HOTKEY, HKM_GETHOTKEY, 0, 0));
            const Hotkey chosen = fromControl(packed);
            if (const wchar_t* why = problem(chosen)) {
                MessageBoxW(dialog, why, kTitle, MB_ICONINFORMATION);
                return TRUE;
            }
            reinterpret_cast<State*>(GetWindowLongPtrW(dialog, DWLP_USER))->chosen = chosen;
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
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_HOTKEY_CLASS;  // Registers msctls_hotkey32, the class the template names.
    InitCommonControlsEx(&controls);

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
