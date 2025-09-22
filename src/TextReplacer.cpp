#include "../include/TextReplacer.h"
#include <windows.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <Ole2.h>
#include <OleAuto.h>
#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <string_view>
#include <algorithm>
#include <vector>

namespace {
class ClipboardBackup {
public:
    ClipboardBackup() = default;
    explicit ClipboardBackup(IDataObject* dataObject) {
        capture(dataObject);
    }

    ClipboardBackup(const ClipboardBackup&) = delete;
    ClipboardBackup& operator=(const ClipboardBackup&) = delete;

    ClipboardBackup(ClipboardBackup&&) noexcept = default;
    ClipboardBackup& operator=(ClipboardBackup&&) noexcept = default;

    ~ClipboardBackup() {
        clear();
    }

    bool empty() const noexcept {
        return entries_.empty();
    }

    bool restore() noexcept {
        if (entries_.empty()) {
            return false;
        }

        if (!OpenClipboard(nullptr)) {
            return false;
        }

        struct ClipboardCloser {
            ~ClipboardCloser() {
                CloseClipboard();
            }
        } closer;

        if (!EmptyClipboard()) {
            return false;
        }

        bool success = true;
        for (auto& entry : entries_) {
            if (!entry.handle) {
                continue;
            }

            if (!SetClipboardData(entry.format, entry.handle)) {
                success = false;
                releaseHandle(entry.tymed, entry.handle);
                entry.handle = nullptr;
            } else {
                entry.handle = nullptr;
            }
        }

        return success;
    }

private:
    struct Entry {
        CLIPFORMAT format{ 0 };
        DWORD tymed{ TYMED_NULL };
        HANDLE handle{ nullptr };
    };

    std::vector<Entry> entries_;

    void capture(IDataObject* dataObject) {
        if (!dataObject) {
            return;
        }

        Microsoft::WRL::ComPtr<IEnumFORMATETC> enumerator;
        if (FAILED(dataObject->EnumFormatEtc(DATADIR_GET, &enumerator))) {
            return;
        }

        FORMATETC format{};
        ULONG fetched = 0;
        while (enumerator->Next(1, &format, &fetched) == S_OK) {
            STGMEDIUM medium{};
            const HRESULT hr = dataObject->GetData(&format, &medium);

            if (format.ptd) {
                CoTaskMemFree(format.ptd);
                format.ptd = nullptr;
            }

            if (FAILED(hr)) {
                continue;
            }

            Entry entry{};
            entry.format = format.cfFormat;
            entry.tymed = medium.tymed;
            entry.handle = duplicateMedium(medium, format.cfFormat);

            ReleaseStgMedium(&medium);

            if (entry.handle) {
                entries_.push_back(entry);
            }
        }
    }

    static HANDLE duplicateMedium(const STGMEDIUM& medium, CLIPFORMAT format) {
        switch (medium.tymed) {
        case TYMED_HGLOBAL:
            return OleDuplicateData(medium.hGlobal, format, 0);
        case TYMED_GDI:
            return OleDuplicateData(medium.hBitmap, format, 0);
        case TYMED_ENHMF:
            return OleDuplicateData(medium.hEnhMetaFile, format, 0);
        case TYMED_MFPICT:
            return OleDuplicateData(medium.hMetaFilePict, format, 0);
        default:
            return nullptr;
        }
    }

    static void releaseHandle(DWORD tymed, HANDLE handle) noexcept {
        if (!handle) {
            return;
        }

        switch (tymed) {
        case TYMED_HGLOBAL:
        case TYMED_MFPICT:
            GlobalFree(handle);
            break;
        case TYMED_GDI:
            DeleteObject(static_cast<HGDIOBJ>(handle));
            break;
        case TYMED_ENHMF:
            DeleteEnhMetaFile(static_cast<HENHMETAFILE>(handle));
            break;
        default:
            break;
        }
    }

    void clear() noexcept {
        for (auto& entry : entries_) {
            if (entry.handle) {
                releaseHandle(entry.tymed, entry.handle);
            }
        }
        entries_.clear();
    }
};
} // namespace

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

std::wstring TextReplacer::getSelectedTextViaUIAutomation() const {
    struct ScopedCoInitializer {
        HRESULT hr{ S_OK };
        bool shouldUninitialize{ false };

        ScopedCoInitializer() {
            hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (hr == RPC_E_CHANGED_MODE) {
                hr = S_OK;
            } else if (SUCCEEDED(hr)) {
                shouldUninitialize = true;
            }
        }

        ~ScopedCoInitializer() {
            if (shouldUninitialize) {
                CoUninitialize();
            }
        }
    } initializer;

    if (FAILED(initializer.hr)) {
        return L"";
    }

    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(automation.GetAddressOf()))) || !automation) {
        return L"";
    }

    Microsoft::WRL::ComPtr<IUIAutomationElement> focusedElement;
    if (FAILED(automation->GetFocusedElement(focusedElement.GetAddressOf())) || !focusedElement) {
        return L"";
    }

    Microsoft::WRL::ComPtr<IUIAutomationTextPattern> textPattern;
    if (SUCCEEDED(focusedElement->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(textPattern.GetAddressOf()))) && textPattern) {
        Microsoft::WRL::ComPtr<IUIAutomationTextRangeArray> selection;
        if (SUCCEEDED(textPattern->GetSelection(selection.GetAddressOf())) && selection) {
            int selectionLength = 0;
            if (SUCCEEDED(selection->get_Length(&selectionLength)) && selectionLength > 0) {
                Microsoft::WRL::ComPtr<IUIAutomationTextRange> range;
                if (SUCCEEDED(selection->GetElement(0, range.GetAddressOf())) && range) {
                    BSTR text = nullptr;
                    if (SUCCEEDED(range->GetText(-1, &text)) && text) {
                        std::wstring result(text, SysStringLen(text));
                        SysFreeString(text);
                        return result;
                    }
                }
            }
        }
    }

    Microsoft::WRL::ComPtr<IUIAutomationValuePattern> valuePattern;
    if (SUCCEEDED(focusedElement->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(valuePattern.GetAddressOf()))) && valuePattern) {
        BSTR value = nullptr;
        if (SUCCEEDED(valuePattern->get_CurrentValue(&value)) && value) {
            std::wstring result(value, SysStringLen(value));
            SysFreeString(value);
            return result;
        }
    }

    return L"";
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

    void* ptr = GlobalLock(hMem);
    if (ptr) {
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

    const bool wasCtrlPressed = isKeyPressed(VK_CONTROL);
    const bool wasShiftPressed = isKeyPressed(VK_SHIFT);
    const bool wasAltPressed = isKeyPressed(VK_MENU);

    bool modifiersReleased = false;
    const auto ensureModifiersReleased = [&]() {
        if (!modifiersReleased) {
            releaseModifierKeys();
            modifiersReleased = true;
        }
    };

    const auto restoreModifiers = [&]() {
        if (!modifiersReleased) {
            return;
        }

        if (wasCtrlPressed) {
            keybd_event(VK_CONTROL, 0, 0, 0);
        }

        if (wasShiftPressed) {
            keybd_event(VK_SHIFT, 0, 0, 0);
        }

        if (wasAltPressed) {
            keybd_event(VK_MENU, 0, 0, 0);
        }
    };

    struct ScopedOleInitializer {
        HRESULT hr{ E_FAIL };
        bool shouldUninitialize{ false };

        ScopedOleInitializer() {
            hr = OleInitialize(nullptr);
            if (hr == RPC_E_CHANGED_MODE) {
                hr = S_OK;
            } else if (SUCCEEDED(hr)) {
                shouldUninitialize = true;
            }
        }

        ~ScopedOleInitializer() {
            if (shouldUninitialize) {
                OleUninitialize();
            }
        }
    } oleInitializer;

    std::unique_ptr<ClipboardBackup> clipboardBackup;
    Microsoft::WRL::ComPtr<IDataObject> originalClipboardData;
    if (SUCCEEDED(oleInitializer.hr)) {
        Microsoft::WRL::ComPtr<IDataObject> dataObject;
        if (SUCCEEDED(OleGetClipboard(dataObject.GetAddressOf()))) {
            auto backup = std::make_unique<ClipboardBackup>(dataObject.Get());
            if (backup && !backup->empty()) {
                clipboardBackup = std::move(backup);
            } else {
                originalClipboardData = std::move(dataObject);
            }
        }
    }

    bool clipboardRestored = false;
    const auto restoreClipboard = [&]() {
        if (clipboardRestored) {
            return;
        }
        clipboardRestored = true;

        bool restored = false;

        if (clipboardBackup && clipboardBackup->restore()) {
            restored = true;
            clipboardBackup.reset();
        } else if (originalClipboardData) {
            if (SUCCEEDED(OleSetClipboard(originalClipboardData.Get()))) {
                restored = true;
            }
        }

        if (!restored) {
            copyToClipboard(initialClipboardText);
        }
    };

    std::wstring selectedText = getSelectedTextViaUIAutomation();

    if (selectedText.empty()) {
        ensureModifiersReleased();
        pressCtrlC();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        selectedText = getTextFromClipboard();
        if (initialClipboardText == selectedText) {
            restoreModifiers();
            restoreClipboard();
            return;
        }
    } else {
        ensureModifiersReleased();
    }

    const std::wstring transformedText = transformText(selectedText);

    copyToClipboard(transformedText);
    pressCtrlV();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    restoreModifiers();
    restoreClipboard();
}
