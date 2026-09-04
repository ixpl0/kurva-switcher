#pragma once

#include <string>

#include "Hotkey.h"

namespace kurva::settings {

// Stored under HKEY_CURRENT_USER\Software\kurva-switcher.
[[nodiscard]] bool switchLayoutAfterConversion();
void setSwitchLayoutAfterConversion(bool enabled);

// The key combination that starts a conversion: Pause unless another one was chosen.
[[nodiscard]] Hotkey hotkey();
void setHotkey(const Hotkey& combination);

// Windows autostart: the per-user Run key, so no installer or administrator rights are needed.
struct Autostart {
    enum class State {
        Off,              // No entry.
        ThisExecutable,   // The entry starts the executable that is running now.
        OtherExecutable,  // The entry starts some other copy (old path, renamed file...).
    };
    State state = State::Off;
    std::wstring registeredPath;  // Executable the entry points to, without quotes.
};

[[nodiscard]] Autostart autostart();

// True only when the entry points to the running executable.
[[nodiscard]] bool runAtStartup();

// Enabling always records the full path of the running executable, replacing any other entry.
bool setRunAtStartup(bool enabled);

// If the entry points to an executable that no longer exists (the build was moved, renamed
// or replaced), points it to the running executable instead. Returns true when it did so.
bool repairAutostart();

// Full path of the running executable.
[[nodiscard]] std::wstring executablePath();

// Running from %TEMP% (typically straight out of a zip archive) - such a path must not be
// recorded for autostart because the file disappears.
[[nodiscard]] bool isTemporaryLocation(const std::wstring& path);

}  // namespace kurva::settings
