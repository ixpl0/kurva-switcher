#include "Dialogs.h"

#include <windows.h>

#include <strsafe.h>

namespace kurva::dialogs {

namespace {

HWND openDialog = nullptr;

}  // namespace

void registerOpen(HWND dialog) {
    openDialog = dialog;
}

void registerClosed(HWND dialog) {
    if (openDialog == dialog) {
        openDialog = nullptr;
    }
}

bool focusOpen() {
    if (!openDialog) {
        return false;
    }
    SetForegroundWindow(openDialog);
    return true;
}

void centerOnMouseMonitor(HWND dialog) {
    POINT cursor{};
    GetCursorPos(&cursor);
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    RECT window{};
    if (!GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor) ||
        !GetWindowRect(dialog, &window)) {
        return;
    }
    const RECT& work = monitor.rcWork;
    const LONG x = work.left + ((work.right - work.left) - (window.right - window.left)) / 2;
    const LONG y = work.top + ((work.bottom - work.top) - (window.bottom - window.top)) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

HFONT deriveFont(HWND dialog, int percent, bool bold, const wchar_t* face) {
    LOGFONTW logFont{};
    const HFONT base = reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0));
    if (!base || GetObjectW(base, sizeof(logFont), &logFont) == 0) {
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0)) {
            return nullptr;
        }
        logFont = metrics.lfMessageFont;
    }
    // The dialog font is already scaled for the monitor's DPI; scaling its height keeps that.
    logFont.lfHeight = MulDiv(logFont.lfHeight, percent, 100);
    logFont.lfWidth = 0;
    if (bold) {
        logFont.lfWeight = FW_SEMIBOLD;
    }
    if (face) {
        StringCchCopyW(logFont.lfFaceName, ARRAYSIZE(logFont.lfFaceName), face);
    }
    return CreateFontIndirectW(&logFont);
}

}  // namespace kurva::dialogs
