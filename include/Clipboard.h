#pragma once

#include <windows.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kurva::clipboard {

// Opens the clipboard, retrying (and pumping messages meanwhile) until it succeeds or
// the timeout expires; closes it on destruction. Pumping matters: while we own the
// clipboard with delayed rendering, another application may be holding it open and
// waiting for *our* WM_RENDERFORMAT answer.
class Lock {
public:
    Lock(HWND owner, std::chrono::milliseconds timeout);
    ~Lock();

    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

    [[nodiscard]] bool isOpen() const noexcept { return open_; }

private:
    bool open_ = false;
};

// Copies bytes into a new movable HGLOBAL suitable for SetClipboardData. nullptr on failure.
[[nodiscard]] HGLOBAL makeGlobal(const void* bytes, size_t size);

// Text in the representation of a clipboard text format (CF_UNICODETEXT, CF_TEXT, CF_OEMTEXT).
[[nodiscard]] HGLOBAL makeTextGlobal(std::wstring_view text, UINT format);

// Marks the current clipboard content as "do not record in the clipboard history (Win+V)
// and do not sync to other devices". The clipboard must be open.
void addHistoryOptOut();

// Reads CF_UNICODETEXT. nullopt if the clipboard could not be opened or holds no text.
[[nodiscard]] std::optional<std::wstring> readUnicodeText(HWND owner, std::chrono::milliseconds timeout);

// A private copy of everything on the clipboard that can be put back later.
class Snapshot {
public:
    Snapshot() = default;
    ~Snapshot();

    Snapshot(Snapshot&& other) noexcept;
    Snapshot& operator=(Snapshot&& other) noexcept;
    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;

    // Copies the current clipboard contents. Returns false if the clipboard could not be opened.
    bool capture(HWND owner, std::chrono::milliseconds timeout);

    [[nodiscard]] bool captured() const noexcept { return captured_; }
    [[nodiscard]] size_t formatCount() const noexcept { return entries_.size(); }

    // Puts the copy back. When requiredOwner is not null, nothing is touched unless that
    // window still owns the clipboard (somebody else may have copied something meanwhile).
    // Returns true when the clipboard now holds the snapshot.
    bool restore(HWND owner, HWND requiredOwner, std::chrono::milliseconds timeout);

private:
    struct Entry {
        UINT format = 0;
        HANDLE handle = nullptr;
        bool isEnhMetafile = false;
    };

    static void freeHandle(Entry& entry) noexcept;
    void release() noexcept;

    std::vector<Entry> entries_;
    bool captured_ = false;
};

}  // namespace kurva::clipboard
