#include <windows.h>
#include <string>
#include <memory>
#include "../include/HotkeyListener.h"
#include "../include/TextReplacer.h"
#include "../resource.h"

namespace {
    constexpr UINT WM_TRAYICON = WM_APP + 1;
    constexpr UINT ID_EXIT = 1001;
    constexpr const char* WINDOW_CLASS_NAME = "KurvaSwitcherWindowClass";
    constexpr const char* TOOLTIP_TEXT = "Kurva Switcher";

    NOTIFYICONDATA nid = { 0 };
    HWND hWnd = nullptr;
    bool running = true;
}

void setupTrayIcon(HINSTANCE hInstance) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

    const size_t tooltipLength = strlen(TOOLTIP_TEXT);
    if (tooltipLength < sizeof(nid.szTip)) {
        strcpy_s(nid.szTip, sizeof(nid.szTip), TOOLTIP_TEXT);
    }

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void removeTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN) {
            POINT pt;
            if (GetCursorPos(&pt)) {
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, "Exit");
                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
                    PostMessage(hwnd, WM_NULL, 0, 0);
                    DestroyMenu(hMenu);
                }
            }
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_EXIT) {
            running = false;
            PostQuitMessage(0);
            return 0;
        }
        break;

    default:
        break;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

bool setupWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        MessageBox(nullptr, "Failed to register window class.", "Error", MB_ICONERROR);
        return false;
    }

    hWnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        "",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hWnd) {
        MessageBox(nullptr, "Failed to create window.", "Error", MB_ICONERROR);
        return false;
    }

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    if (!setupWindow(hInstance)) {
        return 1;
    }

    setupTrayIcon(hInstance);

    auto hotkeyListener = std::make_unique<HotkeyListener>();
    auto textReplacer = std::make_unique<TextReplacer>();

    if (!hotkeyListener->initialize()) {
        MessageBox(nullptr, "Failed to register hotkey.", "Error", MB_ICONERROR);
        removeTrayIcon();
        return 1;
    }

    MSG msg = { 0 };
    while (running && GetMessage(&msg, nullptr, 0, 0) > 0) {
        if (hotkeyListener->isHotkeyPressed(msg)) {
            textReplacer->replaceSelectedText();
        } else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    removeTrayIcon();
    return static_cast<int>(msg.wParam);
}
