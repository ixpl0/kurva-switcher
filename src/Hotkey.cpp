#include "Hotkey.h"

#include <windows.h>

#include <format>

namespace kurva {

namespace {

std::wstring keyName(const Hotkey& hotkey) {
    const UINT key = hotkey.virtualKey;
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) {
        return std::wstring(1, static_cast<wchar_t>(key));
    }
    // GetKeyNameText names a scan code rather than a virtual key, and tells Home from Num 7 by
    // the extended-key bit; both are packed the way WM_KEYDOWN packs them into its lParam.
    // MapVirtualKey has no scan code for Pause, whose E1 1D 45 sequence is missing from the
    // layout tables; GetKeyNameText knows it as 0x45 without the extended bit (with the bit,
    // 0x45 is Num Lock).
    LONG lParam = 0x45 << 16;
    if (key != VK_PAUSE) {
        lParam = static_cast<LONG>(MapVirtualKeyW(key, MAPVK_VK_TO_VSC) << 16);
        if (hotkey.extendedKey) {
            lParam |= 1 << 24;
        }
    }
    wchar_t buffer[64];
    if (GetKeyNameTextW(lParam, buffer, static_cast<int>(ARRAYSIZE(buffer))) > 0) {
        return buffer;
    }
    return std::format(L"key {:#x}", key);
}

}  // namespace

std::wstring Hotkey::name() const {
    std::wstring text;
    if (modifiers & MOD_CONTROL) {
        text += L"Ctrl+";
    }
    if (modifiers & MOD_SHIFT) {
        text += L"Shift+";
    }
    if (modifiers & MOD_ALT) {
        text += L"Alt+";
    }
    if (modifiers & MOD_WIN) {
        text += L"Win+";
    }
    return text + keyName(*this);
}

}  // namespace kurva
