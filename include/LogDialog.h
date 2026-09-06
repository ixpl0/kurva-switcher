#pragma once

#include <windows.h>

namespace kurva::logdialog {

// The "Log..." window: the buffered log lines, updated live while it is open, with a button
// that copies them for a bug report.
void show(HINSTANCE instance, HWND owner);

}  // namespace kurva::logdialog
