#pragma once

#include <windows.h>

#include <string>

namespace kurva {

// A key combination in the terms of RegisterHotKey.
struct Hotkey {
    UINT modifiers = 0;        // MOD_CONTROL, MOD_SHIFT, MOD_ALT.
    UINT virtualKey = 0;       // VK_*; 0 means "no combination".
    bool extendedKey = false;  // Home rather than Num 7, say. Affects the name only:
                               // RegisterHotKey goes by the virtual key.

    [[nodiscard]] bool empty() const noexcept { return virtualKey == 0; }
    [[nodiscard]] bool operator==(const Hotkey&) const noexcept = default;

    // "Ctrl+Shift+Pause". Letters and digits are Latin whatever the current keyboard layout.
    [[nodiscard]] std::wstring name() const;
};

inline constexpr Hotkey kDefaultHotkey{.virtualKey = VK_PAUSE};

}  // namespace kurva
