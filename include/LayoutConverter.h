#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace kurva {

enum class Layout { Unknown, Latin, Cyrillic };

struct Conversion {
    std::wstring text;
    bool changed = false;
    // Layout the *last* converted word ended up in. Used to pick the keyboard layout
    // the user most likely wants to continue typing with.
    Layout lastTarget = Layout::Unknown;
};

// Pure text logic: retypes text as if the other keyboard layout had been active.
//
// Every run of "layout characters" (letters and the punctuation that differs between
// the Russian ЙЦУКЕН and US QWERTY layouts) is treated as one word. A word is converted
// in the direction of its majority; a word with no majority (e.g. only ambiguous
// punctuation such as "?") follows the majority of the whole text.
//
// No Win32 dependencies here so the logic can be unit-tested on any platform.
class LayoutConverter {
public:
    LayoutConverter();

    [[nodiscard]] Conversion convert(std::wstring_view text) const;

    // Layout that produced most of the characters, or Unknown for a tie / no letters.
    [[nodiscard]] Layout dominantLayout(std::wstring_view text) const;

private:
    [[nodiscard]] bool isLayoutChar(wchar_t ch) const;

    std::unordered_map<wchar_t, wchar_t> toLatin_;
    std::unordered_map<wchar_t, wchar_t> toCyrillic_;
    std::unordered_set<wchar_t> cyrillicChars_;
    std::unordered_set<wchar_t> latinChars_;
};

}  // namespace kurva
