#pragma once

#include <windows.h>

namespace kurva::aboutdialog {

// The About box: version, links, and where the settings file is.
void show(HINSTANCE instance, HWND owner);

}  // namespace kurva::aboutdialog
