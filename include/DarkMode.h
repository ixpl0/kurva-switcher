#pragma once

#include <windows.h>

namespace kurva::darkmode {

// Lets the popup menus of this process follow the Windows app mode (Settings > Personalization >
// Colors > "Choose your default app mode"): dark menus when it is Dark, light ones otherwise.
// Call once, before the first menu is shown. On Windows before 10 1809 the menus stay light.
void allowDarkMenus();

// Pass every WM_SETTINGCHANGE here: when the app mode is switched while the program runs, the
// next menu is drawn in the new colors.
void handleSettingChange(LPARAM lParam);

}  // namespace kurva::darkmode
