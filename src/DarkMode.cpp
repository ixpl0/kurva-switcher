#include "DarkMode.h"

#include <windows.h>

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

}  // namespace

void allowDarkMenus() {
    const Api& fn = api();
    if (!fn.available()) {
        log::info(L"dark mode: not available on this Windows build, menus stay light");
        return;
    }
    fn.setPreferredAppMode(kAllowDark);
    fn.refreshImmersiveColorPolicyState();
    fn.flushMenuThemes();
    log::info(L"dark mode: menus follow the Windows app mode");
}

void handleSettingChange(LPARAM lParam) {
    const Api& fn = api();
    const auto* setting = reinterpret_cast<const wchar_t*>(lParam);
    if (!fn.available() || !setting || lstrcmpiW(setting, L"ImmersiveColorSet") != 0) {
        return;
    }
    fn.refreshImmersiveColorPolicyState();
    fn.flushMenuThemes();
}

}  // namespace kurva::darkmode
