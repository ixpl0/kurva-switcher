#pragma once

#include <windows.h>

#include <optional>

#include "Hotkey.h"

namespace kurva::hotkeydialog {

struct Request {
    Hotkey current;                   // Prefills the field.
    Hotkey other;                     // The other slot: one combination cannot serve twice.
    const wchar_t* prompt = nullptr;  // The line above the field.
    const wchar_t* hint = nullptr;    // The lines below the field.
    bool allowEmpty = false;          // Backspace and OK switch the slot off: the empty hotkey comes back.
};

// Shows the "press the key combination" dialog and returns the combination the user chose, or
// nothing after Cancel. What comes back has passed the checks: it is not a plain typing key, it
// is not `other`, and no other program has registered it. The caller must have unregistered its
// own hotkeys first: a registered combination never reaches the field.
[[nodiscard]] std::optional<Hotkey> ask(HINSTANCE instance, HWND owner, const Request& request);

}  // namespace kurva::hotkeydialog
