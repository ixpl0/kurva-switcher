#ifndef TEXTREPLACER_H
#define TEXTREPLACER_H

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <memory>

class TextReplacer {
public:
    TextReplacer();
    ~TextReplacer() = default;

    TextReplacer(const TextReplacer&) = delete;
    TextReplacer& operator=(const TextReplacer&) = delete;
    TextReplacer(TextReplacer&&) = delete;
    TextReplacer& operator=(TextReplacer&&) = delete;

    void replaceSelectedText();

private:
    void releaseModifierKeys() const;
    void pressCtrlC() const;
    void pressCtrlV() const;
    std::wstring getTextFromClipboard() const;
    bool isKeyPressed(int vkCode) const;
    void copyToClipboard(const std::wstring& text) const;
    std::wstring transformText(const std::wstring& originalText) const;
    std::wstring getSelectedTextViaUIAutomation() const;

    bool isMostlyFirstLanguage(
        const std::wstring& word,
        const std::unordered_set<wchar_t>& set1,
        const std::unordered_set<wchar_t>& set2
    ) const;

    std::wstring transformWord(
        const std::wstring& word,
        const std::unordered_map<wchar_t, wchar_t>& charMap
    ) const;

    void initializeCharacterMaps();

    std::unordered_set<wchar_t> russianChars;
    std::unordered_set<wchar_t> englishChars;
    std::unordered_map<wchar_t, wchar_t> rusToEng;
    std::unordered_map<wchar_t, wchar_t> engToRus;
};

#endif // TEXTREPLACER_H
