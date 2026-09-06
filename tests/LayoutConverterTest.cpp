// Self-contained checks for the pure conversion logic. No test framework needed:
//   cl /std:c++20 /utf-8 /EHsc /Iinclude tests\LayoutConverterTest.cpp src\LayoutConverter.cpp
//   g++ -std=c++20 -Iinclude tests/LayoutConverterTest.cpp src/LayoutConverter.cpp
//
// The layouts here are fixtures: the main block of the keyboard as its four rows, unshifted
// then shifted, so that slot i is the same key in every layout. The program reads the same
// data from Windows (src/KeyboardLayouts.cpp; tests/KeyboardLayoutsTest.cpp checks that).
#include "LayoutConverter.h"

#include <cstdio>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using kurva::Conversion;
using kurva::LayoutChars;
using kurva::LayoutConverter;
using kurva::LayoutIndex;

int failures = 0;
int checks = 0;

LayoutChars layout(std::initializer_list<std::wstring_view> rows) {
    LayoutChars chars;
    for (const std::wstring_view row : rows) {
        chars.insert(chars.end(), row.begin(), row.end());
    }
    return chars;
}

// US QWERTY, Russian, Ukrainian (Enhanced) and German (QWERTZ), as Windows lays them out.
const LayoutChars us = layout({L"`1234567890-=", L"qwertyuiop[]\\", L"asdfghjkl;'", L"zxcvbnm,./",
                               L"~!@#$%^&*()_+", L"QWERTYUIOP{}|", L"ASDFGHJKL:\"", L"ZXCVBNM<>?"});
const LayoutChars ru = layout({L"ё1234567890-=", L"йцукенгшщзхъ\\", L"фывапролджэ", L"ячсмитьбю.",
                               L"Ё!\"№;%:?*()_+", L"ЙЦУКЕНГШЩЗХЪ/", L"ФЫВАПРОЛДЖЭ", L"ЯЧСМИТЬБЮ,"});
const LayoutChars uk = layout({L"'1234567890-=", L"йцукенгшщзхїґ", L"фівапролджє", L"ячсмитьбю.",
                               L"₴!\"№;%:?*()_+", L"ЙЦУКЕНГШЩЗХЇҐ", L"ФІВАПРОЛДЖЄ", L"ЯЧСМИТЬБЮ,"});
const LayoutChars de = layout({L"^1234567890ß´", L"qwertzuiopü+#", L"asdfghjklöä", L"yxcvbnm,.-",
                               L"°!\"§$%&/()=?`", L"QWERTZUIOPÜ*'", L"ASDFGHJKLÖÄ", L"YXCVBNM;:_"});

// Positions in the lists the converters below are built from.
constexpr LayoutIndex kUs = 0;
constexpr LayoutIndex kRu = 1;
constexpr LayoutIndex kUk = 2;
constexpr LayoutIndex kDe = 1;

constexpr std::optional<LayoutIndex> kNone = std::nullopt;

std::string codePoints(std::wstring_view text) {
    std::string out;
    char buffer[16];
    for (const wchar_t ch : text) {
        std::snprintf(buffer, sizeof(buffer), "U+%04X ", static_cast<unsigned>(ch));
        out += buffer;
    }
    return out;
}

std::string name(std::optional<LayoutIndex> layout) {
    return layout ? std::to_string(*layout) : std::string("none");
}

void expect(const LayoutConverter& converter, const char* label, std::wstring_view input,
            std::optional<LayoutIndex> active, std::wstring_view expected,
            std::optional<LayoutIndex> expectedLastTarget) {
    ++checks;
    const Conversion actual = converter.convert(input, active);
    const bool expectedChanged = expected != input;
    if (actual.text != expected || actual.changed != expectedChanged || actual.lastTarget != expectedLastTarget) {
        ++failures;
        std::printf("FAIL %s\n  input:    %s(active %s)\n  expected: %s(changed=%d, last=%s)\n  actual:   %s(changed=%d, last=%s)\n",
                    label, codePoints(input).c_str(), name(active).c_str(), codePoints(expected).c_str(),
                    expectedChanged ? 1 : 0, name(expectedLastTarget).c_str(), codePoints(actual.text).c_str(),
                    actual.changed ? 1 : 0, name(actual.lastTarget).c_str());
    }
}

void expectDominant(const LayoutConverter& converter, const char* label, std::wstring_view input,
                    std::optional<LayoutIndex> expected) {
    ++checks;
    const std::optional<LayoutIndex> actual = converter.dominantLayout(input);
    if (actual != expected) {
        ++failures;
        std::printf("FAIL %s\n  input: %s\n  expected %s, got %s\n", label, codePoints(input).c_str(),
                    name(expected).c_str(), name(actual).c_str());
    }
}

void expectTrue(const char* label, bool condition) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL %s\n", label);
    }
}

}  // namespace

int main() {
    expectTrue("the fixtures describe the same keys", us.size() == ru.size() && us.size() == uk.size() && us.size() == de.size());

    // The everyday case: Russian and English.
    const LayoutConverter russian({us, ru});
    expectTrue("two layouts", russian.layoutCount() == 2);
    expect(russian, "latin word to cyrillic", L"ghbdtn", kNone, L"привет", kRu);
    expect(russian, "cyrillic word to latin", L"привет", kNone, L"ghbdtn", kUs);
    expect(russian, "upper case", L"GHBDTN", kNone, L"ПРИВЕТ", kRu);
    expect(russian, "sentence typed in the wrong layout", L"Ghbdtn? rfr ltkf&", kNone, L"Привет, как дела?", kRu);
    expect(russian, "sentence back", L"Привет, как дела?", kNone, L"Ghbdtn? rfr ltkf&", kUs);
    expect(russian, "each word follows its own majority", L"hello привет", kNone, L"руддщ ghbdtn", kUs);
    expect(russian, "last changed word decides the layout", L"привет hello", kNone, L"ghbdtn руддщ", kRu);
    expect(russian, "digits and spaces are kept", L"ghbdtn 123 vbh", kNone, L"привет 123 мир", kRu);
    expect(russian, "nothing to convert", L"123 - 456", kNone, L"123 - 456", kNone);
    expect(russian, "empty input", L"", kNone, L"", kNone);
    expect(russian, "line breaks survive", L"ghbdtn\r\nvbh", kNone, L"привет\r\nмир", kRu);
    expect(russian, "ambiguous punctuation follows the text", L"ghbdtn ?", kNone, L"привет ,", kRu);
    expect(russian, "ambiguous punctuation alone follows the active layout", L"?", kUs, L",", kRu);
    expect(russian, "ambiguous punctuation alone, the other way round", L"?", kRu, L"&", kUs);
    expect(russian, "ambiguous punctuation alone, no active layout: the first layout", L"?", kNone, L",", kRu);
    expect(russian, "punctuation on letter keys", L"Hello, world!", kNone, L"Руддщб цщкдв!", kRu);
    expect(russian, "yo and tilde", L"`~", kNone, L"ёЁ", kRu);
    expect(russian, "brackets", L"[]{}", kNone, L"хъХЪ", kRu);
    expect(russian, "shifted digits", L"@#$^&", kNone, L"\"№;:?", kRu);
    expect(russian, "shifted digits back", L"\"№;:?", kNone, L"@#$^&", kUs);
    expect(russian, "slash keys", L"/|?", kNone, L"./,", kRu);
    expect(russian, "quotes", L"\"ghbdtn\"", kNone, L"ЭприветЭ", kRu);
    expect(russian, "apostrophe", L"don't", kNone, L"вщтэе", kRu);
    expect(russian, "non-layout characters pass through", L"ghbdtn 🐸 (vbh)", kNone, L"привет 🐸 (мир)", kRu);
    expect(russian, "the active layout is the target when the word was not typed in it", L"ghbdtn", kRu, L"привет", kRu);
    expect(russian, "the active layout is the source when the word was typed in it", L"ghbdtn", kUs, L"привет", kRu);
    expect(russian, "a tie inside a word follows the text; a character not of that layout stays", L"aй ghbdtn", kRu,
           L"фй привет", kRu);

    expectDominant(russian, "latin", L"hello", kUs);
    expectDominant(russian, "cyrillic", L"привет", kRu);
    expectDominant(russian, "tie", L"aй", kNone);
    expectDominant(russian, "ambiguous only", L".,?", kNone);
    expectDominant(russian, "no layout characters", L"123", kNone);

    // Three layouts, two of them Cyrillic: most Russian words are Ukrainian words key for key.
    const LayoutConverter three({us, ru, uk});
    expect(three, "three: latin word, active latin, goes to the layout that changes it most (the first on a tie)",
           L"ghbdtn", kUs, L"привет", kRu);
    expect(three, "three: latin word, active ukrainian, goes to the active layout", L"ghbdtn", kUk, L"привет", kUk);
    expect(three, "three: shared cyrillic word, active russian, goes to latin", L"привет", kRu, L"ghbdtn", kUs);
    expect(three, "three: shared cyrillic word, active ukrainian, goes to latin", L"привет", kUk, L"ghbdtn", kUs);
    expect(three, "three: shared cyrillic word, active latin", L"привет", kUs, L"ghbdtn", kUs);
    expect(three, "three: russian-only word, active ukrainian", L"ыыы", kUk, L"ііі", kUk);
    expect(three, "three: russian-only word, active russian", L"ыыы", kRu, L"sss", kUs);
    expect(three, "three: ukrainian-only word, active latin", L"її", kUs, L"]]", kUs);
    expect(three, "three: punctuation follows the words", L"ghbdtn ?", kUk, L"привет ,", kUk);
    expect(three, "three: no active layout", L"ghbdtn", kNone, L"привет", kRu);
    expect(three, "three: mixed text", L"hello привет", kRu, L"руддщ ghbdtn", kUs);

    // Two Latin layouts: only the keys that differ count, the rest is not even a word.
    const LayoutConverter german({us, de});
    expect(german, "qwertz: common letters are no layout characters", L"hello", kNone, L"hello", kNone);
    expect(german, "qwertz: swapped letters", L"zoo yes", kNone, L"yoo zes", kDe);
    expect(german, "qwertz: swapped letters, active german", L"zoo yes", kDe, L"yoo zes", kUs);
    expect(german, "qwertz: punctuation", L"[];'", kNone, L"ü+öä", kDe);

    // A key that has a character in one layout only: the character stays, but it belongs to
    // that layout and a word that keeps every character does not count as changed.
    LayoutChars usWithoutRouble = us;
    usWithoutRouble.push_back(L'\0');
    LayoutChars ruWithRouble = ru;
    ruWithRouble.push_back(L'₽');
    const LayoutConverter rouble({usWithoutRouble, ruWithRouble});
    expect(rouble, "no counterpart: the character stays", L"ghbdtn ₽", kNone, L"привет ₽", kRu);
    expect(rouble, "no counterpart: nothing changes", L"₽", kUs, L"₽", kNone);
    expectDominant(rouble, "no counterpart: counts for its layout", L"₽", kRu);

    // Fewer than two layouts: nothing to convert to.
    const LayoutConverter single({us});
    expect(single, "one layout", L"ghbdtn", kUs, L"ghbdtn", kNone);
    const LayoutConverter none;
    expect(none, "no layouts", L"ghbdtn", kNone, L"ghbdtn", kNone);
    expectTrue("no layouts: count", none.layoutCount() == 0);

    // An active layout the converter does not know is ignored.
    expect(russian, "unknown active layout", L"ghbdtn", LayoutIndex{7}, L"привет", kRu);

    std::printf("%d check(s), %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
