#pragma once

#include <windows.h>

#include <uiautomation.h>
#include <wrl/client.h>

#include <string>

namespace kurva {

struct Selection {
    enum class Status {
        Unsupported,  // The focused control does not expose its selection; use Ctrl+C.
        Empty,        // The control says nothing is selected.
        Text,         // Selected text was read without touching the clipboard.
    };

    Status status = Status::Unsupported;
    std::wstring text;
    // Process that owns the focused control (0 if unknown). It is the process that will
    // read the clipboard when we send Ctrl+V.
    DWORD processId = 0;
    // Web engines put focus on hidden proxy inputs (Google Docs, code editors, ...), so
    // "nothing selected" from them is not to be trusted; other frameworks report the
    // real selection and their "Empty" means we can safely skip Ctrl+C.
    bool emptyIsTrusted = true;
};

// Reads the selected text of the focused control through UI Automation.
// COM must be initialized on the calling thread before the reader is created.
class SelectionReader {
public:
    SelectionReader();

    SelectionReader(const SelectionReader&) = delete;
    SelectionReader& operator=(const SelectionReader&) = delete;

    [[nodiscard]] bool available() const noexcept { return automation_ != nullptr; }

    [[nodiscard]] Selection read();

private:
    Microsoft::WRL::ComPtr<IUIAutomation> automation_;
};

}  // namespace kurva
