#include "../include/TextReplacer.h"
#include <windows.h>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <unordered_set>

std::wstring transformText(const std::wstring& originalText);

void TextReplacer::replaceSelectedText() {
    if (OpenClipboard(nullptr)) {
        // save initial clipboard
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

        // copy text
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('C', 0, 0, 0);
        keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(50);

        if (OpenClipboard(nullptr)) {
            // get text from clipboard
            HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
            if (hClipboardData) {
                wchar_t* pchData = static_cast<wchar_t*>(GlobalLock(hClipboardData));
                if (pchData) {
                    std::wstring originalText(pchData);
                    GlobalUnlock(hClipboardData);

                    // change text layout
                    std::wstring transformedText = transformText(originalText);

                    EmptyClipboard();
                    // set converted text to clipboard
                    HGLOBAL hNewClipboardData = GlobalAlloc(GMEM_DDESHARE, static_cast<DWORD>((transformedText.size() + 1) * sizeof(wchar_t)));
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
            CloseClipboard();
        }

        // paste converted text
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event('V', 0, 0, 0);
        keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
        Sleep(50);

        // restore initial clipboard
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hNewOriginalClipboardData = GlobalAlloc(GMEM_MOVEABLE, (originalClipboardText.size() + 1) * sizeof(wchar_t));
            if (hNewOriginalClipboardData) {
                wchar_t* pNewOriginalClipboardData = static_cast<wchar_t*>(GlobalLock(hNewOriginalClipboardData));
                if (pNewOriginalClipboardData) {
                    wcscpy_s(pNewOriginalClipboardData, originalClipboardText.size() + 1, originalClipboardText.c_str());
                    GlobalUnlock(hNewOriginalClipboardData);
                    SetClipboardData(CF_UNICODETEXT, hNewOriginalClipboardData);
                }
            }
            CloseClipboard();
        }
    }
}

bool isMostlyFirstLanguage(const std::wstring& originalText, const std::wstring& chars1, const std::wstring& chars2) {
    int count1 = 0, count2 = 0;

    std::unordered_set<wchar_t> rusSet(chars1.begin(), chars1.end());
    std::unordered_set<wchar_t> engSet(chars2.begin(), chars2.end());

    for (wchar_t ch : originalText) {
        if (rusSet.count(ch)) count1++;
        if (engSet.count(ch)) count2++;
    }

    return count1 > count2;
}


std::wstring transformText(const std::wstring& originalText) {
    std::wstring chars1 = L"йцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ\"№;:?.,/";
    std::wstring chars2 = L"qwertyuiop[]asdfghjkl;'zxcvbnm,.`QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>~@#$^&/?|";

    bool isFirstCharset = isMostlyFirstLanguage(originalText, chars1, chars2);

    std::unordered_map<wchar_t, wchar_t> charMap;
    const std::wstring& fromChars = isFirstCharset ? chars1 : chars2;
    const std::wstring& toChars = isFirstCharset ? chars2 : chars1;

    for (size_t i = 0; i < fromChars.length(); ++i) {
        charMap[fromChars[i]] = toChars[i];
    }

    std::wstring result;
    for (const wchar_t& ch : originalText) {
        auto it = charMap.find(ch);
        if (it != charMap.end()) {
            result += it->second;
        }
        else {
            result += ch;
        }
    }

    return result;
}
