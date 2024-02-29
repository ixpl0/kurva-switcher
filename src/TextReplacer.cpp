#include "../include/TextReplacer.h"
#include <windows.h>
#include <string>
#include <unordered_map>
#include <algorithm>

std::wstring transformText(const std::wstring& originalText);

void TextReplacer::replaceSelectedText() {
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
                std::wstring originalText(pchData);
                GlobalUnlock(hClipboardData);

                std::wstring transformedText = transformText(originalText);

                EmptyClipboard();
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

    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('V', 0, 0, 0);
    keybd_event('V', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
}

std::wstring transformText(const std::wstring& originalText) {
    std::wstring rusLayout = L"йцукенгшщзхъфывапролджэячсмитьбюёЙЦУКЕНГШЩЗХЪФЫВАПРОЛДЖЭЯЧСМИТЬБЮЁ/№";
    std::wstring engLayout = L"qwertyuiop[]asdfghjkl;'zxcvbnm,.`QWERTYUIOP{}ASDFGHJKL:\"ZXCVBNM<>~?#";
    std::unordered_map<wchar_t, wchar_t> rusToEng;
    std::unordered_map<wchar_t, wchar_t> engToRus;

    for (size_t i = 0; i < rusLayout.length(); ++i) {
        rusToEng[rusLayout[i]] = engLayout[i];
        engToRus[engLayout[i]] = rusLayout[i];
    }

    std::wstring result;
    result.reserve(originalText.length());

    for (wchar_t ch : originalText) {
        auto rusIt = rusToEng.find(ch);
        auto engIt = engToRus.find(ch);

        if (rusIt != rusToEng.end()) {
            result += rusIt->second;
        }
        else if (engIt != engToRus.end()) {
            result += engIt->second;
        }
        else {
            result += ch;
        }
    }

    return result;
}
