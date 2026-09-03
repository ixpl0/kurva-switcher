#pragma once

#include <windows.h>

namespace kurva::input {

// Tag carried in KBDLLHOOKSTRUCT::dwExtraInfo of every key event we synthesize.
inline constexpr ULONG_PTR kInjectedTag = 0x4B555256;  // "KURV"

// Sends a key-up for every modifier (Shift, Ctrl, Alt, Win, left and right) that Windows
// currently considers pressed, so that our Ctrl+C / Ctrl+V is not seen as, say,
// Ctrl+Shift+V. Returns the number of keys released.
int releaseModifiers();

// Presses and releases Ctrl+<virtualKey> as one atomic SendInput batch.
bool sendCtrlChord(WORD virtualKey);

}  // namespace kurva::input
