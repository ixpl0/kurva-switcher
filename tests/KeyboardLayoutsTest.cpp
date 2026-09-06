// Checks that what Windows reports for the US and Russian layouts is what kurva-switcher used
// to carry in a built-in table, and that conversions through the reported layouts work.
// Windows only; no test framework needed:
//   cl /std:c++20 /utf-8 /EHsc /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Iinclude tests\KeyboardLayoutsTest.cpp src\KeyboardLayouts.cpp src\LayoutConverter.cpp user32.lib
//
// The two layouts are loaded if the machine does not have them; whatever this test loads is
// unloaded again at the end.
#include "KeyboardLayouts.h"
#include "LayoutConverter.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using kurva::Conversion;
using kurva::LayoutChars;
using kurva::LayoutConverter;
using kurva::LayoutIndex;

int failures = 0;
int checks = 0;

constexpr LayoutIndex kUs = 0;
constexpr LayoutIndex kRu = 1;

// {character on the Russian layout, character on the same key of the US layout} for every key
// of the main block that differs between the two. L'\0' is a key with nothing on it.
const std::vector<std::pair<wchar_t, wchar_t>> kExpectedPairs{
    // Letters.
    {L'й', L'q'}, {L'ц', L'w'}, {L'у', L'e'}, {L'к', L'r'}, {L'е', L't'}, {L'н', L'y'},
    {L'г', L'u'}, {L'ш', L'i'}, {L'щ', L'o'}, {L'з', L'p'}, {L'х', L'['}, {L'ъ', L']'},
    {L'ф', L'a'}, {L'ы', L's'}, {L'в', L'd'}, {L'а', L'f'}, {L'п', L'g'}, {L'р', L'h'},
    {L'о', L'j'}, {L'л', L'k'}, {L'д', L'l'}, {L'ж', L';'}, {L'э', L'\''},
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
    // The rouble sign on AltGr+8 of the Russian layout has no US counterpart.
    {L'₽', L'\0'},
};

struct LoadedLayout {
    HKL handle = nullptr;
    bool loadedHere = false;
};

LoadedLayout load(const wchar_t* layoutId) {
    const std::vector<HKL> before = kurva::keyboard::installedLayouts();
    LoadedLayout result;
    result.handle = LoadKeyboardLayoutW(layoutId, KLF_NOTELLSHELL);
    result.loadedHere = result.handle && std::find(before.begin(), before.end(), result.handle) == before.end();
    return result;
}

void unload(const LoadedLayout& layout) {
    if (layout.loadedHere) {
        UnloadKeyboardLayout(layout.handle);
    }
}

std::string utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    const int length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), length, out.data(), size, nullptr, nullptr);
    return out;
}

std::string shown(wchar_t ch) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "U+%04X", static_cast<unsigned>(ch));
    return ch == L'\0' ? std::string("nothing") : utf8(std::wstring_view(&ch, 1)) + " " + buffer;
}

void expectTrue(const char* label, bool condition) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("FAIL %s\n", label);
    }
}

void expectConversion(const LayoutConverter& converter, const char* label, std::wstring_view input,
                      std::optional<LayoutIndex> active, std::wstring_view expected,
                      std::optional<LayoutIndex> expectedLastTarget) {
    ++checks;
    const Conversion actual = converter.convert(input, active);
    const bool expectedChanged = expected != input;
    if (actual.text != expected || actual.changed != expectedChanged || actual.lastTarget != expectedLastTarget) {
        ++failures;
        std::printf("FAIL %s\n  expected: %s (changed=%d, last=%s)\n  actual:   %s (changed=%d, last=%s)\n", label,
                    utf8(expected).c_str(), expectedChanged ? 1 : 0,
                    expectedLastTarget ? std::to_string(*expectedLastTarget).c_str() : "none",
                    utf8(actual.text).c_str(), actual.changed ? 1 : 0,
                    actual.lastTarget ? std::to_string(*actual.lastTarget).c_str() : "none");
    }
}

}  // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);

    const LoadedLayout us = load(L"00000409");
    const LoadedLayout ru = load(L"00000419");
    if (!us.handle || !ru.handle) {
        std::printf("cannot load the US (%p) or the Russian (%p) keyboard layout\n", static_cast<void*>(us.handle),
                    static_cast<void*>(ru.handle));
        return 1;
    }
    std::printf("US layout: %s\nRussian layout: %s\n", utf8(kurva::keyboard::name(us.handle)).c_str(),
                utf8(kurva::keyboard::name(ru.handle)).c_str());
    expectTrue("the US layout is named after its language", kurva::keyboard::name(us.handle).starts_with(L"en-US ("));
    expectTrue("the Russian layout is named after its language", kurva::keyboard::name(ru.handle).starts_with(L"ru-RU ("));

    const LayoutChars usChars = kurva::keyboard::readChars(us.handle);
    const LayoutChars ruChars = kurva::keyboard::readChars(ru.handle);
    expectTrue("both layouts describe the same keys", usChars.size() == ruChars.size() && !usChars.empty());

    // Every key that differs, as {Russian, US}; keys that agree (digits...) are not interesting.
    std::set<std::pair<wchar_t, wchar_t>> actualPairs;
    for (size_t slot = 0; slot < std::min(usChars.size(), ruChars.size()); ++slot) {
        if (usChars[slot] != ruChars[slot]) {
            actualPairs.emplace(ruChars[slot], usChars[slot]);
        }
    }
    const std::set<std::pair<wchar_t, wchar_t>> expectedPairs(kExpectedPairs.begin(), kExpectedPairs.end());
    for (const auto& pair : expectedPairs) {
        ++checks;
        if (!actualPairs.contains(pair)) {
            ++failures;
            std::printf("FAIL missing: Russian %s on the key of US %s\n", shown(pair.first).c_str(), shown(pair.second).c_str());
        }
    }
    for (const auto& pair : actualPairs) {
        ++checks;
        if (!expectedPairs.contains(pair)) {
            ++failures;
            std::printf("FAIL unexpected: Russian %s on the key of US %s\n", shown(pair.first).c_str(), shown(pair.second).c_str());
        }
    }

    const LayoutConverter converter({usChars, ruChars});
    expectConversion(converter, "latin word to cyrillic", L"ghbdtn", kUs, L"привет", kRu);
    expectConversion(converter, "sentence back", L"Привет, как дела?", kRu, L"Ghbdtn? rfr ltkf&", kUs);
    expectConversion(converter, "shifted digits", L"@#$^&", std::nullopt, L"\"№;:?", kRu);
    expectConversion(converter, "slash keys", L"/|?", std::nullopt, L"./,", kRu);
    expectConversion(converter, "yo and tilde", L"`~", std::nullopt, L"ёЁ", kRu);
    expectConversion(converter, "the rouble sign stays", L"100 ₽", kUs, L"100 ₽", std::nullopt);
    expectConversion(converter, "the numeric keypad is not consulted", L"1.5", kRu, L"1/5", kUs);

    unload(ru);
    unload(us);

    std::printf("%d check(s), %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
