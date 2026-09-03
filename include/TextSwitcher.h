#pragma once

#include <windows.h>

#include <chrono>
#include <set>
#include <string>

#include "Clipboard.h"
#include "LayoutConverter.h"
#include "Selection.h"

namespace kurva {

// Converts the selected text of the foreground application into the other keyboard layout.
//
// The fragile part is the clipboard round-trip. Sending Ctrl+C / Ctrl+V and sleeping for a
// fixed 50 ms works only when the target application happens to react faster than that;
// otherwise the conversion silently does nothing, or - worse - the *previous* clipboard
// content gets pasted because we restored it before the application had read ours.
//
// This class removes the guesswork:
//   * the selection is read through UI Automation whenever the control exposes it, so most
//     applications never see a Ctrl+C at all;
//   * otherwise the clipboard is emptied, Ctrl+C is sent and we wait for the clipboard
//     *sequence number* to change: the only reliable sign that the copy has happened;
//   * the converted text is published with *delayed rendering*: Windows calls us back
//     (WM_RENDERFORMAT) at the very moment the target reads the clipboard, so the original
//     clipboard is restored only after the paste has actually been served;
//   * if another application replaces the clipboard meanwhile, nothing is restored over it.
class TextSwitcher {
public:
    explicit TextSwitcher(HINSTANCE instance);
    ~TextSwitcher();

    TextSwitcher(const TextSwitcher&) = delete;
    TextSwitcher& operator=(const TextSwitcher&) = delete;

    [[nodiscard]] bool isReady() const noexcept { return window_ != nullptr; }

    void setSwitchKeyboardLayout(bool enabled) noexcept { switchLayout_ = enabled; }

    // Performs one conversion. Calls made while a conversion is in flight are ignored.
    void run();

private:
    struct Target {
        HWND foreground = nullptr;
        HWND focus = nullptr;
        std::set<DWORD> processIds;  // Every process that may read the clipboard on Ctrl+V.
    };

    struct CopyResult {
        enum class Outcome {
            ClipboardUnavailable,  // Could not even open the clipboard; nothing was touched.
            NothingCopied,         // Ctrl+C changed nothing: the clipboard is empty now.
            Copied,                // The application put something on the clipboard.
        };
        Outcome outcome = Outcome::ClipboardUnavailable;
        std::wstring text;    // CF_UNICODETEXT of what was copied (may be empty).
        HWND owner = nullptr; // Clipboard owner right after the copy.
    };

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void onRenderFormat(UINT format);
    void onRenderAllFormats();
    void onDestroyClipboard();

    [[nodiscard]] Target captureTarget(DWORD focusedProcessId) const;
    [[nodiscard]] CopyResult copySelectionWithCtrlC();
    enum class PublishResult { NotTouched, Emptied, Published };
    [[nodiscard]] PublishResult publish(const std::wstring& text);
    void waitForPaste();
    void restoreClipboard(HWND requiredOwner);
    void switchKeyboardLayout(const Target& target, Layout layout) const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    bool classRegistered_ = false;

    SelectionReader selection_;
    LayoutConverter converter_;

    bool busy_ = false;
    bool switchLayout_ = true;

    // State of the conversion in flight.
    std::wstring published_;      // Text we render on demand while we own the clipboard.
    bool ownsClipboard_ = false;  // We put published_ on the clipboard (delayed rendering).
    bool clipboardTaken_ = false; // Somebody else emptied the clipboard after we published.
    std::set<DWORD> targetProcessIds_;
    int targetRenders_ = 0;
    int unknownRenders_ = 0;
    int foreignRenders_ = 0;
    std::chrono::steady_clock::time_point lastRelevantRender_{};
    clipboard::Snapshot backup_;
};

}  // namespace kurva
