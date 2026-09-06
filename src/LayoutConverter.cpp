#include "LayoutConverter.h"

#include <algorithm>
#include <utility>

namespace kurva {

namespace {

bool contains(const std::vector<LayoutIndex>& list, LayoutIndex value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

}  // namespace

LayoutConverter::LayoutConverter(std::vector<LayoutChars> layouts) {
    const size_t layoutCount = layouts.size();
    slotOf_.resize(layoutCount);

    size_t slotCount = 0;
    for (const LayoutChars& layout : layouts) {
        slotCount = std::max(slotCount, layout.size());
    }
    for (size_t slot = 0; slot < slotCount; ++slot) {
        std::vector<wchar_t> chars(layoutCount, L'\0');
        for (size_t index = 0; index < layoutCount; ++index) {
            if (slot < layouts[index].size()) {
                chars[index] = layouts[index][slot];
            }
        }
        // A key that produces the same character in every layout (a digit, say) tells
        // nothing about the layout and has nothing to convert to.
        const bool identical =
            std::all_of(chars.begin(), chars.end(), [&chars](wchar_t ch) { return ch == chars.front(); });
        if (identical) {
            continue;
        }
        const size_t kept = slots_.size();
        for (size_t index = 0; index < layoutCount; ++index) {
            if (chars[index] != L'\0') {
                slotOf_[index].try_emplace(chars[index], kept);  // The first key producing it wins.
            }
        }
        slots_.push_back(std::move(chars));
    }
}

bool LayoutConverter::isLayoutChar(wchar_t ch) const {
    return std::any_of(slotOf_.begin(), slotOf_.end(), [ch](const auto& chars) { return chars.contains(ch); });
}

std::vector<size_t> LayoutConverter::countByLayout(std::wstring_view text) const {
    std::vector<size_t> counts(slotOf_.size(), 0);
    for (const wchar_t ch : text) {
        for (size_t index = 0; index < slotOf_.size(); ++index) {
            if (slotOf_[index].contains(ch)) {
                ++counts[index];
            }
        }
    }
    return counts;
}

std::vector<LayoutIndex> LayoutConverter::leaders(std::wstring_view text) const {
    const std::vector<size_t> counts = countByLayout(text);
    std::vector<LayoutIndex> result;
    const auto best = std::max_element(counts.begin(), counts.end());
    if (best == counts.end() || *best == 0) {
        return result;
    }
    for (size_t index = 0; index < counts.size(); ++index) {
        if (counts[index] == *best) {
            result.push_back(index);
        }
    }
    return result;
}

std::optional<LayoutIndex> LayoutConverter::dominantLayout(std::wstring_view text) const {
    const std::vector<LayoutIndex> leading = leaders(text);
    if (leading.size() == 1) {
        return leading.front();
    }
    return std::nullopt;
}

LayoutIndex LayoutConverter::pickSource(std::wstring_view word, const std::vector<LayoutIndex>& textLeaders,
                                        std::optional<LayoutIndex> active) const {
    std::vector<LayoutIndex> candidates = leaders(word);
    if (candidates.empty()) {
        return 0;  // Not reached: a word is made of layout characters.
    }
    if (candidates.size() > 1) {
        // The rest of the text may know better: keep the candidates that lead there too.
        std::vector<LayoutIndex> narrowed;
        for (const LayoutIndex candidate : candidates) {
            if (contains(textLeaders, candidate)) {
                narrowed.push_back(candidate);
            }
        }
        if (!narrowed.empty()) {
            candidates = std::move(narrowed);
        }
    }
    // Still a tie: the text was most likely typed in the layout that is active right now.
    if (candidates.size() > 1 && active && contains(candidates, *active)) {
        return *active;
    }
    return candidates.front();
}

LayoutIndex LayoutConverter::pickTarget(std::wstring_view word, LayoutIndex source,
                                        std::optional<LayoutIndex> active) const {
    // The user has already switched to the layout they meant.
    if (active && *active != source) {
        return *active;
    }
    // Otherwise the layout that changes the most characters, the first one on a tie: a
    // Cyrillic word goes to the Latin layout rather than to a second Cyrillic one.
    std::optional<LayoutIndex> best;
    size_t bestChanges = 0;
    for (LayoutIndex candidate = 0; candidate < layoutCount(); ++candidate) {
        if (candidate == source) {
            continue;
        }
        size_t changes = 0;
        for (const wchar_t ch : word) {
            if (retype(ch, source, candidate) != ch) {
                ++changes;
            }
        }
        if (!best || changes > bestChanges) {
            best = candidate;
            bestChanges = changes;
        }
    }
    return best.value_or(source);
}

wchar_t LayoutConverter::retype(wchar_t ch, LayoutIndex source, LayoutIndex target) const {
    const auto slot = slotOf_[source].find(ch);
    if (slot == slotOf_[source].end()) {
        return ch;
    }
    const wchar_t retyped = slots_[slot->second][target];
    return retyped != L'\0' ? retyped : ch;
}

Conversion LayoutConverter::convert(std::wstring_view text, std::optional<LayoutIndex> active) const {
    Conversion result;
    result.text.reserve(text.size());
    if (layoutCount() < 2) {
        result.text = text;
        return result;
    }
    if (active && *active >= layoutCount()) {
        active.reset();
    }

    const std::vector<LayoutIndex> textLeaders = leaders(text);

    size_t position = 0;
    while (position < text.size()) {
        if (!isLayoutChar(text[position])) {
            result.text += text[position];
            ++position;
            continue;
        }

        size_t end = position;
        while (end < text.size() && isLayoutChar(text[end])) {
            ++end;
        }
        const std::wstring_view word = text.substr(position, end - position);
        position = end;

        const LayoutIndex source = pickSource(word, textLeaders, active);
        const LayoutIndex target = pickTarget(word, source, active);
        bool wordChanged = false;
        for (const wchar_t ch : word) {
            const wchar_t retyped = retype(ch, source, target);
            result.text += retyped;
            wordChanged = wordChanged || retyped != ch;
        }
        if (wordChanged) {
            result.changed = true;
            result.lastTarget = target;
        }
    }

    return result;
}

}  // namespace kurva
