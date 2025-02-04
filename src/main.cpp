#include <windows.h>
#include "../include/HotkeyListener.h"
#include "../include/TextReplacer.h"
#include "../resource.h"

#define WM_TRAYICON (WM_APP + 1)
#define ID_EXIT 1001

const char* WINDOW_CLASS_NAME = "DummyWindowClass";
const char* TOOLTIP_TEXT = "Kurva Switcher";

NOTIFYICONDATA nid = { 0 };
HWND hWnd;
bool running = true;

void setupTrayIcon(HINSTANCE hInstance) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    strcpy_s(nid.szTip, TOOLTIP_TEXT);

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void removeTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();

            if (hMenu) {
                InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_EXIT, "Exit");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
                PostMessage(hWnd, WM_NULL, 0, 0);
                DestroyMenu(hMenu);
            }
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_EXIT) {
            running = false;
            PostQuitMessage(0);
        }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

bool setupWindow(HINSTANCE hInstance) {
    WNDCLASS wc = { 0 };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Failed to register window class.", "Error", MB_ICONERROR);
        return false;
    }

    hWnd = CreateWindowEx(0, WINDOW_CLASS_NAME, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBox(NULL, "Failed to create window.", "Error", MB_ICONERROR);
        return false;
    }

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!setupWindow(hInstance)) {
        return 1;
    }

    setupTrayIcon(hInstance);

    HotkeyListener hotkeyListener;
    TextReplacer textReplacer;

    if (!hotkeyListener.initialize()) {
        MessageBox(NULL, "Failed to register hotkey.", "Error", MB_ICONERROR);
        return 1;
    }

    MSG msg = { 0 };

    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (hotkeyListener.isHotkeyPressed(msg)) {
            textReplacer.replaceSelectedText();
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    removeTrayIcon();

    return (int)msg.wParam;
}
