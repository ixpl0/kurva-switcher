#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "LayoutConverter.h"

namespace kurva::keyboard {

struct InstalledLayout {
    HKL handle = nullptr;
    std::wstring name;  // "ru-RU (0x04190419)", for the log.
    LayoutChars chars;
};

// The keyboard layouts installed for the user, in the order Windows keeps them.
[[nodiscard]] std::vector<HKL> installedLayouts();

// What every key of the main block produces in each of the layouts, asked from Windows itself
// (ToUnicodeEx) rather than taken from a built-in table, so any pair of installed layouts
// converts: Russian and English, Ukrainian, German, Dvorak...
[[nodiscard]] std::vector<InstalledLayout> describe(const std::vector<HKL>& layouts);

// The same for one layout. The key slots are the same for every layout, as the converter
// expects.
[[nodiscard]] LayoutChars readChars(HKL layout);

// "ru-RU (0x04190419)": the language of the layout and its handle.
[[nodiscard]] std::wstring name(HKL layout);

}  // namespace kurva::keyboard
