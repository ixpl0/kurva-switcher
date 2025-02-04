#include "../include/TextReplacer.h"
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

// Проверка, больше ли символов из первой раскладки
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

// Трансформация слова согласно карте символов
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

// Трансформация текста: обработка раскладки
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

// Сброс нажатых модификаторов
void TextReplacer::releaseModifierKeys() const {
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
}

// Проверка нажатия клавиши
bool TextReplacer::isKeyPressed(int VK_CODE) const {
    return (GetKeyState(VK_CODE) & 0x8000) != 0;
}

// Основной метод замены выделенного текста
void TextReplacer::replaceSelectedText() {
    if (OpenClipboard(nullptr)) {
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

        releaseModifierKeys();

        // Копируем текст
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('C', 0, 0, 0);
        keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(50);

        if (OpenClipboard(nullptr)) {
            HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
            if (hClipboardData) {
                wchar_t* pchData = static_cast<wchar_t*>(GlobalLock(hClipboardData));
                if (pchData) {
                    std::wstring clipboardText(pchData);
                    GlobalUnlock(hClipboardData);

                    if (originalClipboardText != clipboardText) {
                        std::wstring transformedText = transformText(clipboardText);

                        EmptyClipboard();
                        HGLOBAL hNewClipboardData = GlobalAlloc(GMEM_DDESHARE, (transformedText.size() + 1) * sizeof(wchar_t));
                        if (hNewClipboardData) {
                            wchar_t* pNewClipboardData = static_cast<wchar_t*>(GlobalLock(hNewClipboardData));
                            if (pNewClipboardData) {
                                wcscpy_s(pNewClipboardData, transformedText.size() + 1, transformedText.c_str());
                                GlobalUnlock(hNewClipboardData);
                                SetClipboardData(CF_UNICODETEXT, hNewClipboardData);
                            }
                        }
                    }
                }
            }
            CloseClipboard();
        }
    }
}
