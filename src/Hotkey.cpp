#include "Hotkey.h"

#include <windows.h>

#include <cstdint>
#include <format>

namespace kurva {

namespace {

std::wstring modifierPrefix(UINT modifiers) {
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
    return text;
}

bool isLetterOrDigit(UINT key) {
    return (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z');
}

std::wstring keyName(const Hotkey& hotkey) {
    const UINT key = hotkey.virtualKey;
    if (isLetterOrDigit(key)) {
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

// The fixed names of the settings file. Letters and digits are their own names; a key that is
// not listed is written as its virtual key in hex, "0xAD" say.
struct KeyToken {
    UINT virtualKey;
    bool extendedKey;
    const wchar_t* name;
};

constexpr KeyToken kKeyTokens[] = {
    {VK_PAUSE, false, L"Pause"},
    {VK_CANCEL, true, L"Break"},
    {VK_SCROLL, false, L"ScrollLock"},
    {VK_SNAPSHOT, true, L"PrintScreen"},
    {VK_APPS, true, L"Apps"},
    {VK_F1, false, L"F1"},
    {VK_F2, false, L"F2"},
    {VK_F3, false, L"F3"},
    {VK_F4, false, L"F4"},
    {VK_F5, false, L"F5"},
    {VK_F6, false, L"F6"},
    {VK_F7, false, L"F7"},
    {VK_F8, false, L"F8"},
    {VK_F9, false, L"F9"},
    {VK_F10, false, L"F10"},
    {VK_F11, false, L"F11"},
    {VK_F12, false, L"F12"},
    {VK_F13, false, L"F13"},
    {VK_F14, false, L"F14"},
    {VK_F15, false, L"F15"},
    {VK_F16, false, L"F16"},
    {VK_F17, false, L"F17"},
    {VK_F18, false, L"F18"},
    {VK_F19, false, L"F19"},
    {VK_F20, false, L"F20"},
    {VK_F21, false, L"F21"},
    {VK_F22, false, L"F22"},
    {VK_F23, false, L"F23"},
    {VK_F24, false, L"F24"},
    {VK_INSERT, true, L"Insert"},
    {VK_DELETE, true, L"Delete"},
    {VK_HOME, true, L"Home"},
    {VK_END, true, L"End"},
    {VK_PRIOR, true, L"PageUp"},
    {VK_NEXT, true, L"PageDown"},
    {VK_LEFT, true, L"Left"},
    {VK_RIGHT, true, L"Right"},
    {VK_UP, true, L"Up"},
    {VK_DOWN, true, L"Down"},
    {VK_TAB, false, L"Tab"},
    {VK_SPACE, false, L"Space"},
    {VK_RETURN, false, L"Enter"},
    {VK_RETURN, true, L"NumEnter"},
    {VK_ESCAPE, false, L"Escape"},
    {VK_BACK, false, L"Backspace"},
    {VK_CAPITAL, false, L"CapsLock"},
    {VK_NUMLOCK, true, L"NumLock"},
    {VK_CLEAR, false, L"Clear"},
    {VK_NUMPAD0, false, L"Num0"},
    {VK_NUMPAD1, false, L"Num1"},
    {VK_NUMPAD2, false, L"Num2"},
    {VK_NUMPAD3, false, L"Num3"},
    {VK_NUMPAD4, false, L"Num4"},
    {VK_NUMPAD5, false, L"Num5"},
    {VK_NUMPAD6, false, L"Num6"},
    {VK_NUMPAD7, false, L"Num7"},
    {VK_NUMPAD8, false, L"Num8"},
    {VK_NUMPAD9, false, L"Num9"},
    {VK_MULTIPLY, false, L"NumMultiply"},
    {VK_ADD, false, L"NumAdd"},
    {VK_SUBTRACT, false, L"NumSubtract"},
    {VK_DECIMAL, false, L"NumDecimal"},
    {VK_DIVIDE, true, L"NumDivide"},
    {VK_OEM_1, false, L"Semicolon"},
    {VK_OEM_PLUS, false, L"Equals"},
    {VK_OEM_COMMA, false, L"Comma"},
    {VK_OEM_MINUS, false, L"Minus"},
    {VK_OEM_PERIOD, false, L"Period"},
    {VK_OEM_2, false, L"Slash"},
    {VK_OEM_3, false, L"Backquote"},
    {VK_OEM_4, false, L"LeftBracket"},
    {VK_OEM_5, false, L"Backslash"},
    {VK_OEM_6, false, L"RightBracket"},
    {VK_OEM_7, false, L"Quote"},
    {VK_OEM_102, false, L"Oem102"},
};

bool sameText(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size() || left.size() > static_cast<size_t>(INT32_MAX)) {
        return false;
    }
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

std::wstring_view trim(std::wstring_view text) {
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t')) {
        text.remove_suffix(1);
    }
    return text;
}

std::wstring keyToken(const Hotkey& hotkey) {
    if (isLetterOrDigit(hotkey.virtualKey)) {
        return std::wstring(1, static_cast<wchar_t>(hotkey.virtualKey));
    }
    const KeyToken* sameKey = nullptr;
    for (const KeyToken& token : kKeyTokens) {
        if (token.virtualKey != hotkey.virtualKey) {
            continue;
        }
        if (token.extendedKey == hotkey.extendedKey) {
            return token.name;
        }
        if (!sameKey) {
            sameKey = &token;
        }
    }
    if (sameKey) {
        return sameKey->name;
    }
    return std::format(L"0x{:02X}", hotkey.virtualKey);
}

std::optional<UINT> modifierOf(std::wstring_view token) {
    if (sameText(token, L"Ctrl") || sameText(token, L"Control")) {
        return MOD_CONTROL;
    }
    if (sameText(token, L"Shift")) {
        return MOD_SHIFT;
    }
    if (sameText(token, L"Alt")) {
        return MOD_ALT;
    }
    if (sameText(token, L"Win") || sameText(token, L"Windows")) {
        return MOD_WIN;
    }
    return std::nullopt;
}

std::optional<UINT> hexValue(std::wstring_view digits) {
    if (digits.empty()) {
        return std::nullopt;
    }
    UINT value = 0;
    for (const wchar_t ch : digits) {
        UINT digit = 0;
        if (ch >= L'0' && ch <= L'9') {
            digit = static_cast<UINT>(ch - L'0');
        } else if (ch >= L'a' && ch <= L'f') {
            digit = static_cast<UINT>(ch - L'a') + 10;
        } else if (ch >= L'A' && ch <= L'F') {
            digit = static_cast<UINT>(ch - L'A') + 10;
        } else {
            return std::nullopt;
        }
        value = value * 16 + digit;
        if (value > 0xFF) {
            return std::nullopt;
        }
    }
    return value;
}

bool parseKey(std::wstring_view token, Hotkey& hotkey) {
    if (token.size() == 1) {
        wchar_t ch = token[0];
        if (ch >= L'a' && ch <= L'z') {
            ch = static_cast<wchar_t>(ch - L'a' + L'A');
        }
        if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z')) {
            hotkey.virtualKey = static_cast<UINT>(ch);
            hotkey.extendedKey = false;
            return true;
        }
        return false;
    }
    for (const KeyToken& known : kKeyTokens) {
        if (sameText(token, known.name)) {
            hotkey.virtualKey = known.virtualKey;
            hotkey.extendedKey = known.extendedKey;
            return true;
        }
    }
    if (token.size() > 2 && token[0] == L'0' && (token[1] == L'x' || token[1] == L'X')) {
        if (const std::optional<UINT> value = hexValue(token.substr(2)); value && *value != 0) {
            hotkey.virtualKey = *value;
            hotkey.extendedKey = false;
            return true;
        }
    }
    return false;
}

}  // namespace

std::wstring Hotkey::name() const {
    return modifierPrefix(modifiers) + keyName(*this);
}

std::wstring Hotkey::serialize() const {
    if (empty()) {
        return {};
    }
    return modifierPrefix(modifiers) + keyToken(*this);
}

std::optional<Hotkey> Hotkey::parse(std::wstring_view text) {
    Hotkey result;
    std::wstring_view rest = trim(text);
    if (rest.empty()) {
        return result;
    }
    for (;;) {
        const size_t plus = rest.find(L'+');
        if (plus == std::wstring_view::npos) {
            if (!parseKey(trim(rest), result)) {
                return std::nullopt;
            }
            return result;
        }
        const std::optional<UINT> modifier = modifierOf(trim(rest.substr(0, plus)));
        if (!modifier) {
            return std::nullopt;
        }
        result.modifiers |= *modifier;
        rest.remove_prefix(plus + 1);
    }
}

}  // namespace kurva
