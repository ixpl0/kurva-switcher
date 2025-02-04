#include "../include/TextReplacer.h"
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

bool TextReplacer::isMostlyFirstLanguage(
    const std::wstring& word,
    const std::unordered_set<wchar_t>& set1,
    const std::unordered_set<wchar_t>& set2
) const {
    int count1 = 0, count2 = 0;
    for (wchar_t ch : word) {
        if (set1.count(ch)) count1++;
        if (set2.count(ch)) count2++;
    }
    return count1 > count2;
}

std::wstring TextReplacer::transformWord(
    const std::wstring& word,
    const std::unordered_map<wchar_t, wchar_t>& charMap
) const {
    std::wstring result;
    for (wchar_t ch : word) {
        auto it = charMap.find(ch);
        result += (it != charMap.end()) ? it->second : ch;
    }
    return result;
}

std::wstring TextReplacer::transformText(const std::wstring& originalText) const {
    std::wstring chars1 = L"йцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ\"№;:?.,/";
    std::wstring chars2 = L"qwertyuiop[]asdfghjkl;'zxcvbnm,.`QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>~@#$^&/?|";

    std::unordered_set<wchar_t> rusSet(chars1.begin(), chars1.end());
    std::unordered_set<wchar_t> engSet(chars2.begin(), chars2.end());

    std::unordered_map<wchar_t, wchar_t> rusToEng, engToRus;

    for (size_t i = 0; i < chars1.size(); ++i) {
        rusToEng[chars1[i]] = chars2[i];
        engToRus[chars2[i]] = chars1[i];
    }

	std::wstring result, word;

    for (wchar_t ch : originalText) {
        if (rusSet.count(ch) || engSet.count(ch)) {
            word += ch;
        }
        else {
            if (!word.empty()) {
                bool isRus = isMostlyFirstLanguage(word, rusSet, engSet);
                result += transformWord(word, isRus ? rusToEng : engToRus);
                word.clear();
            }
            result += ch;
        }
    }

    if (!word.empty()) {
        bool isRus = isMostlyFirstLanguage(word, rusSet, engSet);
        result += transformWord(word, isRus ? rusToEng : engToRus);
    }

    return result;
}

void TextReplacer::releaseModifierKeys() const {
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
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

    HANDLE hOriginalClipboardData = GetClipboardData(CF_UNICODETEXT);
    std::wstring originalClipboardText;

    if (hOriginalClipboardData) {
        wchar_t* pchOriginalData = static_cast<wchar_t*>(GlobalLock(hOriginalClipboardData));

        if (pchOriginalData) {
            originalClipboardText = pchOriginalData;
            GlobalUnlock(hOriginalClipboardData);
        }
    }

    CloseClipboard();

    return originalClipboardText;
}

bool TextReplacer::isKeyPressed(int VK_CODE) {
    return (GetKeyState(VK_CODE) & 0x8000) != 0;
}

void TextReplacer::copyToClipboard(const std::wstring& text) {
    if (text.empty()) {
        return;
    }

    size_t size = (text.length() + 1) * sizeof(wchar_t);
    
    if (!OpenClipboard(nullptr)) {
        return;
    }

    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return;
    }

    void* ptr = GlobalLock(hMem);
    memcpy(ptr, text.c_str(), size);
    GlobalUnlock(hMem); 
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
}

void TextReplacer::replaceSelectedText() {
    std::wstring initialClipboardText = getTextFromClipboard();

    bool isCtrlPressed = isKeyPressed(VK_CONTROL);
    bool isShiftPressed = isKeyPressed(VK_SHIFT);
    bool isAltPressed = isKeyPressed(VK_MENU);

	releaseModifierKeys();
    pressCtrlC();
    Sleep(50);

    std::wstring copiedClipboardText = getTextFromClipboard();

    if (initialClipboardText == copiedClipboardText) {
        return;
    }

    std::wstring transformedText = transformText(copiedClipboardText);

    copyToClipboard(transformedText);
    pressCtrlV();
    Sleep(50);

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
