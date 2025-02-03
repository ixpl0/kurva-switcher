#include "../include/TextReplacer.h"
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

bool isMostlyFirstLanguage(const std::wstring& word, const std::unordered_set<wchar_t>& set1, const std::unordered_set<wchar_t>& set2) {
	int count1 = 0, count2 = 0;
	for (wchar_t ch : word) {
		if (set1.count(ch)) count1++;
		if (set2.count(ch)) count2++;
	}
	return count1 > count2;
}

std::wstring transformWord(const std::wstring& word, const std::unordered_map<wchar_t, wchar_t>& charMap) {
	std::wstring result;
	for (wchar_t ch : word) {
		auto it = charMap.find(ch);
		result += (it != charMap.end()) ? it->second : ch;
	}
	return result;
}

std::wstring transformText(const std::wstring& originalText) {
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
	bool isWord = false;
	for (wchar_t ch : originalText) {
		if (rusSet.count(ch) || engSet.count(ch)) {
			word += ch;
			isWord = true;
		}
		else {
			if (!word.empty()) {
				bool isRus = isMostlyFirstLanguage(word, rusSet, engSet);
				result += transformWord(word, isRus ? rusToEng : engToRus);
				word.clear();
			}
			result += ch;
			isWord = false;
		}
	}
	if (!word.empty()) {
		bool isRus = isMostlyFirstLanguage(word, rusSet, engSet);
		result += transformWord(word, isRus ? rusToEng : engToRus);
	}

	return result;
}

void releaseModifierKeys() {
	keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
}

bool isKeyPressed(int VK_CODE) {
	return (GetKeyState(VK_CODE) & 0x8000) != 0;
}

void TextReplacer::replaceSelectedText() {
	if (OpenClipboard(nullptr)) {
		bool isClipboardChanged = false;
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

		bool ctrlPressed = isKeyPressed(VK_CONTROL);
		bool shiftPressed = isKeyPressed(VK_SHIFT);
		bool altPressed = isKeyPressed(VK_MENU);
		releaseModifierKeys();

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
					std::wstring clipboardText(pchData);
					GlobalUnlock(hClipboardData);

					if (originalClipboardText != clipboardText) {
						isClipboardChanged = true;

						// change text layout
						std::wstring transformedText = transformText(clipboardText);

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
			}
			CloseClipboard();
		}

		if (isClipboardChanged) {
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

		if (ctrlPressed) {
			keybd_event(VK_CONTROL, 0, 0, 0);
		}

		if (shiftPressed) {
			keybd_event(VK_SHIFT, 0, 0, 0);
		}

		if (altPressed) {
			keybd_event(VK_MENU, 0, 0, 0);
		}
	}
}
