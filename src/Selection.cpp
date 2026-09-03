#include "Selection.h"

#include <oleauto.h>

#include <string_view>

#include "Log.h"

namespace kurva {

namespace {

using Microsoft::WRL::ComPtr;

// How long a single UI Automation request may block us when an application hangs.
constexpr DWORD kUiaTimeoutMs = 1500;

bool isWebEngine(IUIAutomationElement& element) {
    BSTR framework = nullptr;
    if (FAILED(element.get_CurrentFrameworkId(&framework)) || !framework) {
        return false;
    }
    const std::wstring_view id(framework, SysStringLen(framework));
    const bool web = id == L"Chrome" || id == L"Gecko" || id == L"MicrosoftEdge" || id == L"InternetExplorer";
    SysFreeString(framework);
    return web;
}

}  // namespace

SelectionReader::SelectionReader() {
    // CUIAutomation8 (Windows 8+) lets us bound how long a hung application can stall us.
    ComPtr<IUIAutomation2> automation2;
    HRESULT hr = CoCreateInstance(__uuidof(CUIAutomation8), nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(automation2.GetAddressOf()));
    if (SUCCEEDED(hr) && automation2) {
        automation2->put_ConnectionTimeout(kUiaTimeoutMs);
        automation2->put_TransactionTimeout(kUiaTimeoutMs);
        automation_ = automation2;
        return;
    }

    hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(automation_.GetAddressOf()));
    if (FAILED(hr)) {
        automation_.Reset();
        log::info(L"uia: CoCreateInstance failed (hr={:#x}); selection will always go through Ctrl+C",
                  static_cast<unsigned long>(hr));
    }
}

Selection SelectionReader::read() {
    Selection result;
    if (!automation_) {
        return result;
    }

    ComPtr<IUIAutomationElement> focused;
    HRESULT hr = automation_->GetFocusedElement(focused.GetAddressOf());
    if (FAILED(hr) || !focused) {
        log::info(L"uia: no focused element (hr={:#x})", static_cast<unsigned long>(hr));
        return result;
    }

    int processId = 0;
    if (SUCCEEDED(focused->get_CurrentProcessId(&processId)) && processId > 0) {
        result.processId = static_cast<DWORD>(processId);
    }
    result.emptyIsTrusted = !isWebEngine(*focused.Get());

    ComPtr<IUIAutomationTextPattern> textPattern;
    hr = focused->GetCurrentPatternAs(UIA_TextPatternId, IID_PPV_ARGS(textPattern.GetAddressOf()));
    if (FAILED(hr) || !textPattern) {
        log::info(L"uia: focused element of process {} has no TextPattern", result.processId);
        return result;
    }

    ComPtr<IUIAutomationTextRangeArray> ranges;
    hr = textPattern->GetSelection(ranges.GetAddressOf());
    if (FAILED(hr) || !ranges) {
        log::info(L"uia: GetSelection failed (hr={:#x})", static_cast<unsigned long>(hr));
        return result;
    }

    int count = 0;
    if (FAILED(ranges->get_Length(&count))) {
        count = 0;
    }
    for (int index = 0; index < count; ++index) {
        ComPtr<IUIAutomationTextRange> range;
        if (FAILED(ranges->GetElement(index, range.GetAddressOf())) || !range) {
            continue;
        }
        BSTR text = nullptr;
        if (FAILED(range->GetText(-1, &text)) || !text) {
            continue;
        }
        const std::wstring_view piece(text, SysStringLen(text));
        if (!piece.empty()) {
            // Several ranges = multiple carets in a code editor; one line per caret is what
            // such editors expect to paste back.
            if (!result.text.empty()) {
                result.text += L"\r\n";
            }
            result.text.append(piece);
        }
        SysFreeString(text);
    }

    result.status = result.text.empty() ? Selection::Status::Empty : Selection::Status::Text;
    log::info(L"uia: process {} reports {} selected character(s) in {} range(s){}", result.processId,
              result.text.size(), count, result.emptyIsTrusted ? L"" : L" (web engine)");
    return result;
}

}  // namespace kurva
