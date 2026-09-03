#include "LayoutConverter.h"

#include <array>
#include <utility>

namespace kurva {

namespace {

// {character on the Russian layout, character on the same key of the US layout}.
// Both columns are the characters a key produces with the same Shift state.
constexpr std::array<std::pair<wchar_t, wchar_t>, 74> kKeyPairs{{
    // Top letter row.
    {L'й', L'q'}, {L'ц', L'w'}, {L'у', L'e'}, {L'к', L'r'}, {L'е', L't'}, {L'н', L'y'},
    {L'г', L'u'}, {L'ш', L'i'}, {L'щ', L'o'}, {L'з', L'p'}, {L'х', L'['}, {L'ъ', L']'},
    // Home row.
    {L'ф', L'a'}, {L'ы', L's'}, {L'в', L'd'}, {L'а', L'f'}, {L'п', L'g'}, {L'р', L'h'},
    {L'о', L'j'}, {L'л', L'k'}, {L'д', L'l'}, {L'ж', L';'}, {L'э', L'\''},
    // Bottom row.
    {L'я', L'z'}, {L'ч', L'x'}, {L'с', L'c'}, {L'м', L'v'}, {L'и', L'b'}, {L'т', L'n'},
    {L'ь', L'm'}, {L'б', L','}, {L'ю', L'.'}, {L'ё', L'`'},
    // The same keys with Shift.
    {L'Й', L'Q'}, {L'Ц', L'W'}, {L'У', L'E'}, {L'К', L'R'}, {L'Е', L'T'}, {L'Н', L'Y'},
    {L'Г', L'U'}, {L'Ш', L'I'}, {L'Щ', L'O'}, {L'З', L'P'}, {L'Х', L'{'}, {L'Ъ', L'}'},
    {L'Ф', L'A'}, {L'Ы', L'S'}, {L'В', L'D'}, {L'А', L'F'}, {L'П', L'G'}, {L'Р', L'H'},
    {L'О', L'J'}, {L'Л', L'K'}, {L'Д', L'L'}, {L'Ж', L':'}, {L'Э', L'"'},
    {L'Я', L'Z'}, {L'Ч', L'X'}, {L'С', L'C'}, {L'М', L'V'}, {L'И', L'B'}, {L'Т', L'N'},
    {L'Ь', L'M'}, {L'Б', L'<'}, {L'Ю', L'>'}, {L'Ё', L'~'},
    // Punctuation that lives on different keys in the two layouts.
    {L'"', L'@'}, {L'№', L'#'}, {L';', L'$'}, {L':', L'^'}, {L'?', L'&'},
    {L'.', L'/'}, {L',', L'?'}, {L'/', L'|'},
}};

}  // namespace

LayoutConverter::LayoutConverter() {
    for (const auto& [cyrillic, latin] : kKeyPairs) {
        toLatin_.emplace(cyrillic, latin);
        toCyrillic_.emplace(latin, cyrillic);
        cyrillicChars_.insert(cyrillic);
        latinChars_.insert(latin);
    }
}

bool LayoutConverter::isLayoutChar(wchar_t ch) const {
    return cyrillicChars_.contains(ch) || latinChars_.contains(ch);
}

Layout LayoutConverter::dominantLayout(std::wstring_view text) const {
    size_t cyrillic = 0;
    size_t latin = 0;
    for (const wchar_t ch : text) {
        // Ambiguous punctuation (".", ",", "?", ...) exists in both layouts and counts for both.
        if (cyrillicChars_.contains(ch)) {
            ++cyrillic;
        }
        if (latinChars_.contains(ch)) {
            ++latin;
        }
    }
    if (cyrillic > latin) {
        return Layout::Cyrillic;
    }
    if (latin > cyrillic) {
        return Layout::Latin;
    }
    return Layout::Unknown;
}

Conversion LayoutConverter::convert(std::wstring_view text) const {
    Conversion result;
    result.text.reserve(text.size());

    const Layout overall = dominantLayout(text);

    size_t position = 0;
    while (position < text.size()) {
        if (!isLayoutChar(text[position])) {
            result.text += text[position];
            ++position;
            continue;
        }

        size_t end = position;
        while (end < text.size() && isLayoutChar(text[end])) {
            ++end;
        }
        const std::wstring_view word = text.substr(position, end - position);
        position = end;

        Layout source = dominantLayout(word);
        if (source == Layout::Unknown) {
            source = overall;
        }
        if (source == Layout::Unknown) {
            result.text += word;
            continue;
        }

        const auto& map = (source == Layout::Cyrillic) ? toLatin_ : toCyrillic_;
        for (const wchar_t ch : word) {
            if (const auto it = map.find(ch); it != map.end()) {
                result.text += it->second;
                result.changed = true;
            } else {
                result.text += ch;
            }
        }
        result.lastTarget = (source == Layout::Cyrillic) ? Layout::Latin : Layout::Cyrillic;
    }

    return result;
}

}  // namespace kurva
