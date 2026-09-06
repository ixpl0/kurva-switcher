#pragma once

#include <windows.h>

// What the dialogs of the tray menu have in common.
namespace kurva::dialogs {

// The program shows one modal dialog at a time. Each dialog reports itself here, so that a
// click on the tray icon brings it back instead of opening the menu underneath it.
void registerOpen(HWND dialog);
void registerClosed(HWND dialog);

// Brings the open dialog to the front. False when none is open.
bool focusOpen();

// Moves the dialog to the middle of the monitor with the mouse pointer: the user has just
// clicked the tray menu there, not necessarily on the primary monitor where DS_CENTER puts it.
void centerOnMouseMonitor(HWND dialog);

// The dialog's font at `percent` of its size, optionally bold and in another face (nullptr
// keeps the face). nullptr on failure. The caller deletes it (DeleteObject) once no control
// uses it any more.
[[nodiscard]] HFONT deriveFont(HWND dialog, int percent, bool bold, const wchar_t* face);

}  // namespace kurva::dialogs
