// Self-contained checks for the pure conversion logic. No test framework needed:
//   cl /std:c++20 /utf-8 /EHsc /Iinclude tests\LayoutConverterTest.cpp src\LayoutConverter.cpp
//   g++ -std=c++20 -Iinclude tests/LayoutConverterTest.cpp src/LayoutConverter.cpp
#include "LayoutConverter.h"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

int failures = 0;
int checks = 0;

const kurva::LayoutConverter converter;

std::string codePoints(std::wstring_view text) {
    std::string out;
    char buffer[16];
    for (const wchar_t ch : text) {
        std::snprintf(buffer, sizeof(buffer), "U+%04X ", static_cast<unsigned>(ch));
        out += buffer;
    }
    return out;
}

const char* name(kurva::Layout layout) {
    switch (layout) {
    case kurva::Layout::Latin: return "Latin";
    case kurva::Layout::Cyrillic: return "Cyrillic";
    default: return "Unknown";
    }
}

void expectConversion(const char* label, std::wstring_view input, std::wstring_view expected,
                      kurva::Layout expectedLastTarget) {
    ++checks;
    const kurva::Conversion actual = converter.convert(input);
    const bool expectedChanged = expected != input;
    if (actual.text != expected || actual.changed != expectedChanged || actual.lastTarget != expectedLastTarget) {
        ++failures;
        std::printf("FAIL %s\n  input:    %s\n  expected: %s (changed=%d, last=%s)\n  actual:   %s (changed=%d, last=%s)\n",
                    label, codePoints(input).c_str(), codePoints(expected).c_str(), expectedChanged ? 1 : 0,
                    name(expectedLastTarget), codePoints(actual.text).c_str(), actual.changed ? 1 : 0,
                    name(actual.lastTarget));
    }
}

void expectDominant(const char* label, std::wstring_view input, kurva::Layout expected) {
    ++checks;
    const kurva::Layout actual = converter.dominantLayout(input);
    if (actual != expected) {
        ++failures;
        std::printf("FAIL %s\n  input: %s\n  expected %s, got %s\n", label, codePoints(input).c_str(),
                    name(expected), name(actual));
    }
}

}  // namespace

int main() {
    using kurva::Layout;

    expectConversion("latin word to cyrillic", L"ghbdtn", L"привет", Layout::Cyrillic);
    expectConversion("cyrillic word to latin", L"привет", L"ghbdtn", Layout::Latin);
    expectConversion("upper case", L"GHBDTN", L"ПРИВЕТ", Layout::Cyrillic);
    expectConversion("sentence typed in the wrong layout", L"Ghbdtn? rfr ltkf&", L"Привет, как дела?", Layout::Cyrillic);
    expectConversion("sentence back", L"Привет, как дела?", L"Ghbdtn? rfr ltkf&", Layout::Latin);
    expectConversion("each word follows its own majority", L"hello привет", L"руддщ ghbdtn", Layout::Latin);
    expectConversion("last converted word decides the layout", L"привет hello", L"ghbdtn руддщ", Layout::Cyrillic);
    expectConversion("digits and spaces are kept", L"ghbdtn 123 vbh", L"привет 123 мир", Layout::Cyrillic);
    expectConversion("nothing to convert", L"123 - 456", L"123 - 456", Layout::Unknown);
    expectConversion("empty input", L"", L"", Layout::Unknown);
    expectConversion("line breaks survive", L"ghbdtn\r\nvbh", L"привет\r\nмир", Layout::Cyrillic);
    expectConversion("ambiguous punctuation follows the text", L"ghbdtn ?", L"привет ,", Layout::Cyrillic);
    expectConversion("ambiguous punctuation alone stays", L"?", L"?", Layout::Unknown);
    expectConversion("punctuation on letter keys", L"Hello, world!", L"Руддщб цщкдв!", Layout::Cyrillic);
    expectConversion("yo and tilde", L"`~", L"ёЁ", Layout::Cyrillic);
    expectConversion("brackets", L"[]{}", L"хъХЪ", Layout::Cyrillic);
    expectConversion("shifted digits", L"@#$^&", L"\"№;:?", Layout::Cyrillic);
    expectConversion("shifted digits back", L"\"№;:?", L"@#$^&", Layout::Latin);
    expectConversion("slash keys", L"/|?", L"./,", Layout::Cyrillic);
    expectConversion("quotes", L"\"ghbdtn\"", L"ЭприветЭ", Layout::Cyrillic);
    expectConversion("apostrophe", L"don't", L"вщтэе", Layout::Cyrillic);
    expectConversion("non-layout characters pass through", L"ghbdtn 🐸 (vbh)", L"привет 🐸 (мир)", Layout::Cyrillic);

    expectDominant("latin", L"hello", Layout::Latin);
    expectDominant("cyrillic", L"привет", Layout::Cyrillic);
    expectDominant("tie", L"aй", Layout::Unknown);
    expectDominant("ambiguous only", L".,?", Layout::Unknown);
    expectDominant("no layout characters", L"123", Layout::Unknown);

    std::printf("%d check(s), %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
