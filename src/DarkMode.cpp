#include "DarkMode.h"

#include <windows.h>

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>

#include "Log.h"

namespace kurva::darkmode {

namespace {

// Windows draws Win32 popup menus in the dark theme only for a process that has asked for it.
// There is no documented way to ask; Explorer does it through these uxtheme.dll functions, which
// are exported by ordinal only. They have not changed since Windows 10 1809 (build 17763) and
// Notepad++ and other utilities rely on them the same way. Where they are missing, or on an older
// Windows, nothing is called and the menus simply stay light.
constexpr DWORD kFirstSupportedBuild = 17763;

// Ordinal 135 is SetPreferredAppMode(PreferredAppMode) since 1903; on 1809 the same ordinal is
// AllowDarkModeForApp(BOOL). AllowDark == TRUE == 1, so one call serves both.
constexpr int kAllowDark = 1;

using RefreshImmersiveColorPolicyStateFn = void(WINAPI*)();  // ordinal 104
using SetPreferredAppModeFn = int(WINAPI*)(int);             // ordinal 135
using FlushMenuThemesFn = void(WINAPI*)();                   // ordinal 136

struct Api {
    RefreshImmersiveColorPolicyStateFn refreshImmersiveColorPolicyState = nullptr;
    SetPreferredAppModeFn setPreferredAppMode = nullptr;
    FlushMenuThemesFn flushMenuThemes = nullptr;

    [[nodiscard]] bool available() const {
        return refreshImmersiveColorPolicyState && setPreferredAppMode && flushMenuThemes;
    }
};

// GetVersionEx and the VersionHelpers report an older Windows to a program without a
// compatibility manifest; RtlGetVersion tells the truth.
DWORD windowsBuild() {
    using RtlGetVersionFn = LONG(WINAPI*)(RTL_OSVERSIONINFOW*);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtlGetVersion =
        ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
    RTL_OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (!rtlGetVersion || rtlGetVersion(&info) != 0 || info.dwMajorVersion < 10) {
        return 0;
    }
    return info.dwBuildNumber;
}

const Api& api() {
    static const Api loaded = [] {
        Api result;
        if (windowsBuild() < kFirstSupportedBuild) {
            return result;
        }
        // Never freed: the functions are used for the rest of the process lifetime.
        const HMODULE uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!uxtheme) {
            return result;
        }
        result.refreshImmersiveColorPolicyState =
            reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(104)));
        result.setPreferredAppMode =
            reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135)));
        result.flushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136)));
        return result;
    }();
    return loaded;
}

bool isAppModeChange(LPARAM lParam) {
    const auto* setting = reinterpret_cast<const wchar_t*>(lParam);
    return setting && lstrcmpiW(setting, L"ImmersiveColorSet") == 0;
}

// Dialogs.
//
// A dark dialog is put together from three things. DWM paints the title bar dark when asked
// per window (DwmSetWindowAttribute). Windows draws push buttons and scroll bars dark for a
// control whose theme is set to "DarkMode_Explorer", the theme Explorer uses for its own dark
// windows, once the process has asked for dark mode (allowDarkMenus). Everything else, the
// background, the text and the border of the edit controls, the dialog paints itself, in colors
// close to the ones of the dark Windows 11 dialogs.

constexpr COLORREF kDialogBackground = RGB(32, 32, 32);
constexpr COLORREF kFieldBackground = RGB(45, 45, 45);  // Edit controls.
constexpr COLORREF kText = RGB(224, 224, 224);
constexpr COLORREF kLinkText = RGB(76, 194, 255);
constexpr COLORREF kEdge = RGB(100, 100, 100);  // Border of an edit control...
constexpr COLORREF kFocusedEdge = RGB(155, 155, 155);  // ...and of the one with the focus.

// The DwmSetWindowAttribute attribute for a dark title bar. It is documented as 20 since
// Windows 10 2004 (build 19041); the builds from 1809 up to there took 19 for the same thing.
constexpr DWORD kUseImmersiveDarkMode = 20;
constexpr DWORD kUseImmersiveDarkModeBefore2004 = 19;
constexpr DWORD kBuild2004 = 19041;

constexpr UINT_PTR kBorderSubclass = 1;

// Whether the dialogs are drawn dark right now. Refreshed by applyToDialog; the program shows
// one dialog at a time.
bool dialogsDark = false;

struct Brushes {
    HBRUSH dialog = nullptr;
    HBRUSH field = nullptr;
    HBRUSH edge = nullptr;
    HBRUSH focusedEdge = nullptr;
};

// Never freed: the dialogs use them for the rest of the process lifetime.
const Brushes& brushes() {
    static const Brushes created{
        .dialog = CreateSolidBrush(kDialogBackground),
        .field = CreateSolidBrush(kFieldBackground),
        .edge = CreateSolidBrush(kEdge),
        .focusedEdge = CreateSolidBrush(kFocusedEdge),
    };
    return created;
}

// Settings > Personalization > Colors > "Choose your default app mode", read the way Explorer
// and Notepad++ read it. (uxtheme's ShouldAppsUseDarkMode, ordinal 132, lags behind a switch
// made while the program runs.) A missing value is the default: Light.
bool appModeIsDark() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    const LSTATUS status =
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return status == ERROR_SUCCESS && light == 0;
}

// A high contrast theme chooses every color itself; dark mode does not apply then.
bool highContrast() {
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
           (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool wantDarkDialogs() {
    // Without the uxtheme functions the buttons and scroll bars would stay light, and a dark
    // background around light controls is worse than a light dialog.
    return api().available() && appModeIsDark() && !highContrast();
}

// Windows draws the sunken border of an edit control (WS_EX_CLIENTEDGE) in light colors
// whatever the theme of the control. So the border is painted over after Windows has drawn
// it: the outermost pixel in the edge color, the rest in the field's background.
LRESULT CALLBACK borderProc(HWND edit, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR) {
    switch (message) {
    case WM_NCPAINT: {
        if (!dialogsDark) {
            break;
        }
        DefSubclassProc(edit, message, wParam, lParam);
        RECT window{};
        POINT client{};
        if (!GetWindowRect(edit, &window) || !ClientToScreen(edit, &client)) {
            return 0;
        }
        // The border is as thick as the client area is inset; the scroll bars sit inside it.
        const LONG thickness = std::min(client.x - window.left, client.y - window.top);
        const HDC dc = GetWindowDC(edit);
        if (!dc) {
            return 0;
        }
        const Brushes& brush = brushes();
        const HBRUSH outer = GetFocus() == edit ? brush.focusedEdge : brush.edge;
        RECT frame{0, 0, window.right - window.left, window.bottom - window.top};
        for (LONG pixel = 0; pixel < thickness; ++pixel) {
            FrameRect(dc, &frame, pixel == 0 ? outer : brush.field);
            InflateRect(&frame, -1, -1);
        }
        ReleaseDC(edit, dc);
        return 0;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(edit, borderProc, kBorderSubclass);
        break;
    default:
        break;
    }
    return DefSubclassProc(edit, message, wParam, lParam);
}

BOOL CALLBACK applyToControl(HWND control, LPARAM) {
    wchar_t className[32]{};
    GetClassNameW(control, className, ARRAYSIZE(className));
    const wchar_t* theme = dialogsDark ? L"DarkMode_Explorer" : nullptr;  // nullptr: the default again.
    if (lstrcmpW(className, WC_BUTTON) == 0) {
        SetWindowTheme(control, theme, nullptr);
    } else if (lstrcmpW(className, WC_EDIT) == 0) {
        SetWindowTheme(control, theme, nullptr);  // Dark scroll bars.
        if (!GetWindowSubclass(control, borderProc, kBorderSubclass, nullptr)) {
            SetWindowSubclass(control, borderProc, kBorderSubclass, 0);
        }
    } else if (lstrcmpW(className, WC_LINK) == 0) {
        // A SysLink colors its links itself unless each of them is told to take the color the
        // parent sets in WM_CTLCOLORSTATIC.
        LITEM item{};
        item.mask = LIF_ITEMINDEX | LIF_STATE;
        item.stateMask = LIS_DEFAULTCOLORS;
        item.state = dialogsDark ? LIS_DEFAULTCOLORS : 0;
        for (item.iLink = 0; item.iLink < 16 && SendMessageW(control, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&item));
             ++item.iLink) {
        }
    }
    RedrawWindow(control, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME);
    return TRUE;
}

// Sets the dialog up for the current app mode; call again when the mode changes.
void applyToDialog(HWND dialog) {
    dialogsDark = wantDarkDialogs();
    if (api().available()) {  // Windows 10 1809 or later: the title bar can be dark.
        const BOOL dark = dialogsDark ? TRUE : FALSE;
        const DWORD attribute = windowsBuild() >= kBuild2004 ? kUseImmersiveDarkMode : kUseImmersiveDarkModeBefore2004;
        DwmSetWindowAttribute(dialog, attribute, &dark, sizeof(dark));
    }
    EnumChildWindows(dialog, applyToControl, 0);
    RedrawWindow(dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);
}

}  // namespace

void allowDarkMenus() {
    const Api& fn = api();
    if (!fn.available()) {
        log::info(L"dark mode: not available on this Windows build, menus and dialogs stay light");
        return;
    }
    fn.setPreferredAppMode(kAllowDark);
    fn.refreshImmersiveColorPolicyState();
    fn.flushMenuThemes();
    log::info(L"dark mode: menus and dialogs follow the Windows app mode");
}

void handleSettingChange(LPARAM lParam) {
    const Api& fn = api();
    if (!fn.available() || !isAppModeChange(lParam)) {
        return;
    }
    fn.refreshImmersiveColorPolicyState();
    fn.flushMenuThemes();
}

std::optional<INT_PTR> dialogMessage(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        applyToDialog(dialog);
        return std::nullopt;  // The dialog's own set-up still runs.

    case WM_SETTINGCHANGE:
        if (isAppModeChange(lParam)) {
            applyToDialog(dialog);
        }
        return std::nullopt;

    case WM_THEMECHANGED:  // A high contrast theme switched on or off, say.
        applyToDialog(dialog);
        return std::nullopt;

    case WM_CTLCOLORDLG:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        if (!dialogsDark) {
            return std::nullopt;
        }
        const auto dc = reinterpret_cast<HDC>(wParam);
        wchar_t className[32]{};
        if (message != WM_CTLCOLORDLG) {
            GetClassNameW(reinterpret_cast<HWND>(lParam), className, ARRAYSIZE(className));
        }
        const Brushes& brush = brushes();
        COLORREF text = kText;
        COLORREF background = kDialogBackground;
        HBRUSH backgroundBrush = brush.dialog;
        if (lstrcmpW(className, WC_EDIT) == 0) {  // A read-only edit control asks through WM_CTLCOLORSTATIC.
            background = kFieldBackground;
            backgroundBrush = brush.field;
        } else if (lstrcmpW(className, WC_LINK) == 0) {
            text = kLinkText;
        }
        SetTextColor(dc, text);
        SetBkColor(dc, background);
        return reinterpret_cast<INT_PTR>(backgroundBrush);
    }

    default:
        return std::nullopt;
    }
}

}  // namespace kurva::darkmode
