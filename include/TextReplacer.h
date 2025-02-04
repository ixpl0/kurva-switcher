#ifndef TEXTREPLACER_H
#define TEXTREPLACER_H

#include <string>
#include <unordered_set>
#include <unordered_map>

class TextReplacer {
public:
    void replaceSelectedText();

private:
    void releaseModifierKeys() const;
    void pressCtrlC() const;
    void pressCtrlV() const;
    std::wstring getTextFromClipboard() const;
    bool isKeyPressed(int VK_CODE);
    void copyToClipboard(const std::wstring& text);
    std::wstring transformText(const std::wstring& originalText) const;

    bool isMostlyFirstLanguage(
        const std::wstring& word,
        const std::unordered_set<wchar_t>& set1,
        const std::unordered_set<wchar_t>& set2
    ) const;

    std::wstring transformWord(
        const std::wstring& word,
        const std::unordered_map<wchar_t, wchar_t>& charMap
    ) const;
};

#endif // TEXTREPLACER_H
