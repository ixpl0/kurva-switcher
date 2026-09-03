#pragma once

namespace kurva::settings {

// Stored under HKEY_CURRENT_USER\Software\kurva-switcher.
[[nodiscard]] bool switchLayoutAfterConversion();
void setSwitchLayoutAfterConversion(bool enabled);

// Stored in the per-user Run key, so no installer or administrator rights are needed.
[[nodiscard]] bool runAtStartup();
bool setRunAtStartup(bool enabled);

}  // namespace kurva::settings
