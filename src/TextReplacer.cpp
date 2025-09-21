#include "../include/TextReplacer.h"
#include <windows.h>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <vector>

TextReplacer::TextReplacer() {
    initializeCharacterMaps();
}

void TextReplacer::initializeCharacterMaps() {
    const std::vector<std::pair<wchar_t, wchar_t>> charMapping = {
        {L'й', L'q'}, {L'ц', L'w'}, {L'у', L'e'}, {L'к', L'r'}, {L'е', L't'}, {L'н', L'y'}, {L'г', L'u'}, {L'ш', L'i'}, {L'щ', L'o'}, {L'з', L'p'},
        {L'х', L'['}, {L'ъ', L']'}, {L'ф', L'a'}, {L'ы', L's'}, {L'в', L'd'}, {L'а', L'f'}, {L'п', L'g'}, {L'р', L'h'}, {L'о', L'j'}, {L'л', L'k'},
        {L'д', L'l'}, {L'ж', L';'}, {L'э', L'\''}, {L'я', L'z'}, {L'ч', L'x'}, {L'с', L'c'}, {L'м', L'v'}, {L'и', L'b'}, {L'т', L'n'}, {L'ь', L'm'},
        {L'б', L','}, {L'ю', L'.'}, {L'ё', L'`'},
        {L'Й', L'Q'}, {L'Ц', L'W'}, {L'У', L'E'}, {L'К', L'R'}, {L'Е', L'T'}, {L'Н', L'Y'}, {L'Г', L'U'}, {L'Ш', L'I'}, {L'Щ', L'O'}, {L'З', L'P'},
        {L'Х', L'{'}, {L'Ъ', L'}'}, {L'Ф', L'A'}, {L'Ы', L'S'}, {L'В', L'D'}, {L'А', L'F'}, {L'П', L'G'}, {L'Р', L'H'}, {L'О', L'J'}, {L'Л', L'K'},
        {L'Д', L'L'}, {L'Ж', L':'}, {L'Э', L'\"'}, {L'Я', L'Z'}, {L'Ч', L'X'}, {L'С', L'C'}, {L'М', L'V'}, {L'И', L'B'}, {L'Т', L'N'}, {L'Ь', L'M'},
        {L'Б', L'<'}, {L'Ю', L'>'}, {L'Ё', L'~'},
        {L'\"', L'@'}, {L'№', L'#'}, {L';', L'$'}, {L':', L'^'}, {L'?', L'&'}, {L'.', L'/'}, {L',', L'?'}, {L'/', L'|'}
    };

    for (const auto& [rus, eng] : charMapping) {
        rusToEng[rus] = eng;
        engToRus[eng] = rus;
        russianChars.insert(rus);
        englishChars.insert(eng);
    }
}

bool TextReplacer::isMostlyFirstLanguage(
    const std::wstring& word,
    const std::unordered_set<wchar_t>& set1,
    const std::unordered_set<wchar_t>& set2
) const {
    size_t count1 = 0, count2 = 0;
    for (const wchar_t ch : word) {
        if (set1.contains(ch)) ++count1;
        if (set2.contains(ch)) ++count2;
    }
    return count1 > count2;
}

std::wstring TextReplacer::transformWord(
    const std::wstring& word,
    const std::unordered_map<wchar_t, wchar_t>& charMap
) const {
    std::wstring result;
    result.reserve(word.size());
    for (const wchar_t ch : word) {
        if (const auto it = charMap.find(ch); it != charMap.end()) {
            result += it->second;
        } else {
            result += ch;
        }
    }
    return result;
}

std::wstring TextReplacer::transformText(const std::wstring& originalText) const {
    std::wstring result, word;
    result.reserve(originalText.size());

    for (const wchar_t ch : originalText) {
        if (russianChars.contains(ch) || englishChars.contains(ch)) {
            word += ch;
        } else {
            if (!word.empty()) {
                const bool isRus = isMostlyFirstLanguage(word, russianChars, englishChars);
                result += transformWord(word, isRus ? rusToEng : engToRus);
                word.clear();
            }
            result += ch;
        }
    }

    if (!word.empty()) {
        const bool isRus = isMostlyFirstLanguage(word, russianChars, englishChars);
        result += transformWord(word, isRus ? rusToEng : engToRus);
    }

    return result;
}

void TextReplacer::releaseModifierKeys() const {
    constexpr BYTE modifierKeys[] = { VK_CONTROL, VK_SHIFT, VK_MENU };
    for (const BYTE key : modifierKeys) {
        keybd_event(key, 0, KEYEVENTF_KEYUP, 0);
    }
}

void TextReplacer::pressCtrlC() const {
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

void TextReplacer::pressCtrlV() const {
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

std::wstring TextReplacer::getTextFromClipboard() const {
    if (!OpenClipboard(nullptr)) {
        return L"";
    }

    std::wstring originalClipboardText;
    if (const HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* pchData = static_cast<const wchar_t*>(GlobalLock(hClipboardData))) {
            originalClipboardText = pchData;
            GlobalUnlock(hClipboardData);
        }
    }

    CloseClipboard();
    return originalClipboardText;
}

bool TextReplacer::isKeyPressed(int vkCode) const {
    return (GetKeyState(vkCode) & 0x8000) != 0;
}

void TextReplacer::copyToClipboard(const std::wstring& text) const {
    if (text.empty()) {
        return;
    }

    const size_t size = (text.length() + 1) * sizeof(wchar_t);

    if (!OpenClipboard(nullptr)) {
        return;
    }

    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return;
    }

    if (void* ptr = GlobalLock(hMem)) {
        memcpy(ptr, text.c_str(), size);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    } else {
        GlobalFree(hMem);
    }

    CloseClipboard();
}

void TextReplacer::replaceSelectedText() {
    const std::wstring initialClipboardText = getTextFromClipboard();

    const bool isCtrlPressed = isKeyPressed(VK_CONTROL);
    const bool isShiftPressed = isKeyPressed(VK_SHIFT);
    const bool isAltPressed = isKeyPressed(VK_MENU);

    releaseModifierKeys();
    pressCtrlC();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const std::wstring copiedClipboardText = getTextFromClipboard();

    if (initialClipboardText == copiedClipboardText) {
        return;
    }

    const std::wstring transformedText = transformText(copiedClipboardText);

    copyToClipboard(transformedText);
    pressCtrlV();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (isCtrlPressed) {
        keybd_event(VK_CONTROL, 0, 0, 0);
    }

    if (isShiftPressed) {
        keybd_event(VK_SHIFT, 0, 0, 0);
    }

    if (isAltPressed) {
        keybd_event(VK_MENU, 0, 0, 0);
    }

    copyToClipboard(initialClipboardText);
}
