#pragma once

#include <windows.h>

#include <optional>

#include "Hotkey.h"

namespace kurva::hotkeydialog {

// Shows the "press the key combination" dialog, prefilled with `current`, and returns the
// combination the user chose, or nothing after Cancel. What comes back has passed the checks:
// it is not a plain typing key, and no other program has registered it. The caller must have
// unregistered its own hotkeys first: a registered combination never reaches the field.
[[nodiscard]] std::optional<Hotkey> ask(HINSTANCE instance, HWND owner, const Hotkey& current);

// Brings the dialog to the front while ask() is running; does nothing otherwise.
void focus();

}  // namespace kurva::hotkeydialog
