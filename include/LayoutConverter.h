#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kurva {

// The characters one keyboard layout produces: one per key slot, where a slot is a physical
// key in one modifier state. L'\0' is a slot that yields nothing usable (no character, a dead
// key, a multi-character sequence). Every layout given to one converter uses the same slot
// order, so slot i is the same key with the same modifiers in each of them.
using LayoutChars = std::vector<wchar_t>;

// Position of a layout in the list the converter was built from.
using LayoutIndex = size_t;

struct Conversion {
    std::wstring text;
    bool changed = false;
    // Layout the *last* changed word ended up in: the keyboard layout the user most likely
    // wants to continue typing with.
    std::optional<LayoutIndex> lastTarget;
};

// Pure text logic: retypes text as if another keyboard layout had been active.
//
// Only the keys that produce different characters in different layouts matter; digits, space
// and the like say nothing about the layout and are copied through. Every run of the remaining
// "layout characters" is one word. A word is attributed to the layout that produces most of its
// characters (ties: the layout of the whole text, then the layout active in the target window,
// then the first in the list), and retyped in the target layout: the active one when the word
// was not typed in it (the user has already switched to the layout they meant), otherwise the
// layout that changes the most characters, so a Cyrillic word goes to the Latin layout rather
// than to a second Cyrillic one. Characters without a counterpart in the target stay as they are.
//
// No Win32 dependencies here so the logic can be unit-tested on any platform.
class LayoutConverter {
public:
    LayoutConverter() = default;
    explicit LayoutConverter(std::vector<LayoutChars> layouts);

    [[nodiscard]] size_t layoutCount() const noexcept { return slotOf_.size(); }

    // `active`: the layout the target window is using, if known.
    [[nodiscard]] Conversion convert(std::wstring_view text, std::optional<LayoutIndex> active = std::nullopt) const;

    // Layout that produced most of the characters, or nothing for a tie / no layout characters.
    [[nodiscard]] std::optional<LayoutIndex> dominantLayout(std::wstring_view text) const;

private:
    [[nodiscard]] bool isLayoutChar(wchar_t ch) const;
    // Ambiguous characters (".", "?" and the like exist in several layouts) count for each.
    [[nodiscard]] std::vector<size_t> countByLayout(std::wstring_view text) const;
    // The layouts with the highest count, in list order; empty when nothing counts.
    [[nodiscard]] std::vector<LayoutIndex> leaders(std::wstring_view text) const;
    [[nodiscard]] LayoutIndex pickSource(std::wstring_view word, const std::vector<LayoutIndex>& textLeaders,
                                         std::optional<LayoutIndex> active) const;
    [[nodiscard]] LayoutIndex pickTarget(std::wstring_view word, LayoutIndex source,
                                         std::optional<LayoutIndex> active) const;
    // The character the target layout produces on the key that gives `ch` in the source
    // layout; `ch` itself when there is no such key or the target has nothing on it.
    [[nodiscard]] wchar_t retype(wchar_t ch, LayoutIndex source, LayoutIndex target) const;

    // slots_[slot][layout]: the character of a slot in each layout. Only slots that differ
    // between layouts are kept.
    std::vector<std::vector<wchar_t>> slots_;
    // Per layout: every character it produces and the (first) slot that produces it.
    std::vector<std::unordered_map<wchar_t, size_t>> slotOf_;
};

}  // namespace kurva
