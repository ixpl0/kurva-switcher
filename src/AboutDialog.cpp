#include "AboutDialog.h"

#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <cstddef>
#include <string>

#include "Dialogs.h"
#include "Log.h"
#include "Settings.h"
#include "Version.h"
#include "resource.h"

namespace kurva::aboutdialog {

namespace {

constexpr wchar_t kRepositoryUrl[] = L"https://github.com/ixpl0/kurva-switcher";

struct State {
    HINSTANCE instance = nullptr;
    HICON icon = nullptr;
    HFONT titleFont = nullptr;
};

// The layout of an RT_GROUP_ICON resource; the SDK headers do not declare it.
#pragma pack(push, 2)
struct GroupIconEntry {
    BYTE width;  // 0 stands for 256.
    BYTE height;
    BYTE colorCount;
    BYTE reserved;
    WORD planes;
    WORD bitCount;
    DWORD bytes;
    WORD id;
};
struct GroupIconHeader {
    WORD reserved;
    WORD type;
    WORD count;
};
#pragma pack(pop)

// The largest size the icon resource has that does not exceed `wanted`: an exact frame draws
// crisp, a scaled one does not. 0 when the resource cannot be read.
int largestFrameUpTo(HINSTANCE instance, int wanted) {
    const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDI_ICON1), RT_GROUP_ICON);
    const HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
    const auto* bytes = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    if (!bytes) {
        return 0;
    }
    const DWORD size = SizeofResource(instance, resource);
    if (size < sizeof(GroupIconHeader)) {
        return 0;
    }
    const auto* header = reinterpret_cast<const GroupIconHeader*>(bytes);
    const auto* entries = reinterpret_cast<const GroupIconEntry*>(bytes + sizeof(GroupIconHeader));
    int best = 0;
    for (WORD index = 0; index < header->count; ++index) {
        if (sizeof(GroupIconHeader) + (index + 1u) * sizeof(GroupIconEntry) > size) {
            break;
        }
        const int frame = entries[index].width == 0 ? 256 : entries[index].width;
        if (frame <= wanted && frame > best) {
            best = frame;
        }
    }
    return best;
}

HICON loadFrog(HINSTANCE instance) {
    // Twice the tray icon: 64 px at 100 % scaling.
    const int wanted = GetSystemMetrics(SM_CXICON) * 2;
    int size = largestFrameUpTo(instance, wanted);
    if (size == 0) {
        size = wanted;
    }
    return static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, size, size, 0));
}

void open(const wchar_t* what, const wchar_t* parameters = nullptr) {
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", what, parameters, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        log::info(L"about: cannot open {} (error {})", what, result);
    }
}

void showInFolder(const std::wstring& path) {
    const std::wstring parameters = L"/select,\"" + path + L"\"";
    open(L"explorer.exe", parameters.c_str());
}

INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(dialog, DWLP_USER));
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        state = reinterpret_cast<State*>(lParam);
        state->icon = loadFrog(state->instance);
        if (state->icon) {
            SendDlgItemMessageW(dialog, IDC_ABOUT_ICON, STM_SETICON, reinterpret_cast<WPARAM>(state->icon), 0);
        }
        state->titleFont = dialogs::deriveFont(dialog, 170, true, nullptr);
        if (state->titleFont) {
            SendDlgItemMessageW(dialog, IDC_ABOUT_NAME, WM_SETFONT, reinterpret_cast<WPARAM>(state->titleFont), TRUE);
        }
        SetDlgItemTextW(dialog, IDC_ABOUT_VERSION, L"Version " KURVA_VERSION_WSTRING);
        SetDlgItemTextW(dialog, IDC_ABOUT_SETTINGS_PATH, settings::filePath().c_str());
        dialogs::centerOnMouseMonitor(dialog);
        dialogs::registerOpen(dialog);
        ShowWindow(dialog, SW_SHOW);
        SetForegroundWindow(dialog);
        return TRUE;
    }

    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header->code != NM_CLICK && header->code != NM_RETURN) {
            return FALSE;
        }
        const auto* link = reinterpret_cast<const NMLINK*>(lParam);
        switch (header->idFrom) {
        case IDC_ABOUT_LINK:
            open(kRepositoryUrl);
            break;
        case IDC_ABOUT_SETTINGS_LINKS:
            if (lstrcmpW(link->item.szID, L"folder") == 0) {
                showInFolder(settings::filePath());
            } else {
                open(settings::filePath().c_str());
            }
            break;
        default:
            return FALSE;
        }
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, LOWORD(wParam));
            return TRUE;
        }
        return FALSE;

    case WM_DESTROY:
        dialogs::registerClosed(dialog);
        if (state) {
            if (state->titleFont) {
                DeleteObject(state->titleFont);
                state->titleFont = nullptr;
            }
            if (state->icon) {
                DestroyIcon(state->icon);
                state->icon = nullptr;
            }
        }
        return FALSE;

    default:
        return FALSE;
    }
}

}  // namespace

void show(HINSTANCE instance, HWND owner) {
    State state{.instance = instance};
    if (DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), owner, dialogProc, reinterpret_cast<LPARAM>(&state)) == -1) {
        log::info(L"about: the dialog cannot be shown (error {})", GetLastError());
    }
}

}  // namespace kurva::aboutdialog
