#pragma once

#include <windows.h>

#include <optional>

// Dark mode for the menus and the dialogs. Win32 menus and dialogs are light by default; the
// ones of this program follow the Windows app mode (Settings > Personalization > Colors >
// "Choose your default app mode") on Windows 10 1809 and later.
namespace kurva::darkmode {

// Lets the popup menus of this process follow the app mode: dark menus when it is Dark, light
// ones otherwise. The same request is what lets Windows draw the buttons and scroll bars of
// the dialogs dark. Call once, before the first window or menu exists. On Windows before
// 10 1809 everything stays light.
void allowDarkMenus();

// Pass every WM_SETTINGCHANGE of the main window here: when the app mode is switched while
// the program runs, the next menu is drawn in the new colors.
void handleSettingChange(LPARAM lParam);

// The dialogs share their dark mode: call this first in a dialog procedure and return the
// value when there is one. It sets the dialog up dark or light on WM_INITDIALOG (the dialog's
// own WM_INITDIALOG code still runs), follows an app mode switched while the dialog is open
// (WM_SETTINGCHANGE, WM_THEMECHANGED), and answers the WM_CTLCOLOR* messages of a dark dialog
// with its colors. It knows the controls the dialogs of this program use: static text and
// icons, edit controls, push buttons and SysLinks.
[[nodiscard]] std::optional<INT_PTR> dialogMessage(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);

}  // namespace kurva::darkmode
