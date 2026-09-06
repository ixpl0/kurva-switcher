#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace kurva {

// A key combination in the terms of RegisterHotKey.
struct Hotkey {
    UINT modifiers = 0;        // MOD_CONTROL, MOD_SHIFT, MOD_ALT, MOD_WIN.
    UINT virtualKey = 0;       // VK_*; 0 means "no combination".
    bool extendedKey = false;  // Home rather than Num 7, say. Affects the name only:
                               // RegisterHotKey goes by the virtual key.

    [[nodiscard]] bool empty() const noexcept { return virtualKey == 0; }

    // The same combination as far as RegisterHotKey is concerned; the extended-key bit is cosmetic.
    [[nodiscard]] bool operator==(const Hotkey& other) const noexcept {
        return modifiers == other.modifiers && virtualKey == other.virtualKey;
    }

    // "Ctrl+Shift+Pause", for the user. Letters and digits are Latin whatever the current keyboard
    // layout; the other keys are named the way the layout names them ("Num 7", "Page Up").
    [[nodiscard]] std::wstring name() const;

    // "Ctrl+Shift+Pause", for the settings file: fixed English names that depend neither on the
    // keyboard layout nor on the Windows language, "" for no combination.
    [[nodiscard]] std::wstring serialize() const;

    // Reads serialize()'s form back, leniently: any case, spaces around the plus signs. A blank
    // text is the empty hotkey; a text that is not a hotkey at all gives nothing.
    [[nodiscard]] static std::optional<Hotkey> parse(std::wstring_view text);
};

inline constexpr Hotkey kDefaultHotkey{.virtualKey = VK_PAUSE};

}  // namespace kurva
