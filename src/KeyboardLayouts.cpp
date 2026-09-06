#include "KeyboardLayouts.h"

#include <algorithm>
#include <array>
#include <format>

namespace kurva::keyboard {

namespace {

// The keys of the main block, by scan code (set 1): the digit row, the three letter rows, and
// the extra keys of 102-key, Brazilian and Japanese keyboards. The navigation keys and the
// numeric keypad are left out: they produce the same characters in every layout, or repeat
// characters of the main block, which would give a character two keys to be mapped from.
constexpr std::array<UINT, 50> kScanCodes{
    0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x2B,
    0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x56, 0x73, 0x7D,
};

// The modifier states of one key, in slot order: plain, Shift, AltGr (Ctrl+Alt), Shift+AltGr.
struct Modifiers {
    bool shift;
    bool altGr;
};

constexpr std::array<Modifiers, 4> kModifiers{{{false, false}, {true, false}, {false, true}, {true, true}}};

// ToUnicodeEx flag: leave the dead-key state of the thread alone (Windows 10 1607 and later).
constexpr UINT kKeepKeyboardState = 0x4;

constexpr UINT kSpaceScanCode = 0x39;

std::array<BYTE, 256> keyState(Modifiers modifiers) {
    std::array<BYTE, 256> state{};
    if (modifiers.shift) {
        state[VK_SHIFT] = 0x80;
        state[VK_LSHIFT] = 0x80;
    }
    if (modifiers.altGr) {
        state[VK_CONTROL] = 0x80;
        state[VK_LCONTROL] = 0x80;
        state[VK_MENU] = 0x80;
        state[VK_RMENU] = 0x80;
    }
    return state;
}

// One UTF-16 unit that is neither a control character nor half of a surrogate pair.
bool usable(wchar_t ch) {
    return ch >= 0x20 && ch != 0x7F && !(ch >= 0xD800 && ch <= 0xDFFF);
}

wchar_t charOf(HKL layout, UINT scanCode, Modifiers modifiers) {
    const UINT virtualKey = MapVirtualKeyExW(scanCode, MAPVK_VSC_TO_VK_EX, layout);
    if (virtualKey == 0) {
        return L'\0';
    }
    const std::array<BYTE, 256> state = keyState(modifiers);
    wchar_t buffer[8] = {};
    const int produced = ToUnicodeEx(virtualKey, scanCode, state.data(), buffer, 8, kKeepKeyboardState, layout);
    // -1 is a dead key; the buffer then holds the accent it stands for, which is what the key
    // cap shows and what the other layout's character on that key swaps with.
    if (produced != 1 && produced != -1) {
        return L'\0';
    }
    return usable(buffer[0]) ? buffer[0] : L'\0';
}

// A dead key typed into one of our own windows leaves this thread waiting for the character
// to combine it with, and ToUnicodeEx would combine it with the keys probed here. Typing a
// space settles it; without a pending dead key the call changes nothing.
void settleDeadKeys(HKL layout) {
    const std::array<BYTE, 256> state{};
    wchar_t buffer[8] = {};
    ToUnicodeEx(VK_SPACE, kSpaceScanCode, state.data(), buffer, 8, 0, layout);
}

}  // namespace

std::vector<HKL> installedLayouts() {
    const int count = GetKeyboardLayoutList(0, nullptr);
    if (count <= 0) {
        return {};
    }
    std::vector<HKL> layouts(static_cast<size_t>(count));
    const int fetched = GetKeyboardLayoutList(count, layouts.data());
    layouts.resize(static_cast<size_t>(std::max(fetched, 0)));
    return layouts;
}

std::vector<InstalledLayout> describe(const std::vector<HKL>& layouts) {
    std::vector<InstalledLayout> result;
    result.reserve(layouts.size());
    for (const HKL layout : layouts) {
        result.push_back(InstalledLayout{.handle = layout, .name = name(layout), .chars = readChars(layout)});
    }
    return result;
}

LayoutChars readChars(HKL layout) {
    settleDeadKeys(layout);
    LayoutChars chars;
    chars.reserve(kScanCodes.size() * kModifiers.size());
    for (const UINT scanCode : kScanCodes) {
        for (const Modifiers modifiers : kModifiers) {
            chars.push_back(charOf(layout, scanCode, modifiers));
        }
    }
    return chars;
}

std::wstring name(HKL layout) {
    const auto value = reinterpret_cast<ULONG_PTR>(layout);
    // The low word of a layout handle is the language of the layout.
    const LCID locale = MAKELCID(LOWORD(value), SORT_DEFAULT);
    wchar_t tag[LOCALE_NAME_MAX_LENGTH] = {};
    const int length = LCIDToLocaleName(locale, tag, LOCALE_NAME_MAX_LENGTH, 0);  // Counts the terminator.
    const std::wstring language =
        length > 1 ? std::wstring(tag, static_cast<size_t>(length - 1)) : std::wstring(L"unknown language");
    return std::format(L"{} ({:#010x})", language, static_cast<unsigned long>(value & 0xFFFFFFFF));
}

}  // namespace kurva::keyboard
