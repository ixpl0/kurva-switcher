#pragma once

#include <string>

#include "Hotkey.h"

namespace kurva::settings {

// Everything is kept in kurva-switcher.ini next to the executable, so the program is portable:
// copy the folder and the settings come along, delete it and nothing is left behind. When that
// folder cannot be written (Program Files, a read-only share), the file goes to
// %LOCALAPPDATA%\kurva-switcher instead. The registry is touched by the autostart entry only.

// Picks the file, moves the settings of older builds out of the registry and writes the
// defaults for whatever is missing. Call once at startup, before anything else here.
void initialize();

// Full path of the settings file.
[[nodiscard]] std::wstring filePath();

// Off: no hotkey is registered and the tray icon shows it.
[[nodiscard]] bool enabled();
void setEnabled(bool value);

[[nodiscard]] bool switchLayoutAfterConversion();
void setSwitchLayoutAfterConversion(bool value);

// Two combinations start a conversion: Pause and Shift+Pause unless others were chosen.
// Either may be empty.
inline constexpr size_t kHotkeySlots = 2;
[[nodiscard]] Hotkey hotkey(size_t slot);
void setHotkey(size_t slot, const Hotkey& combination);

// Whether the "running in the system tray" notice of the first start has been shown.
[[nodiscard]] bool welcomeShown();
void setWelcomeShown(bool shown);

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
