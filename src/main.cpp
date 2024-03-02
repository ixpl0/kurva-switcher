#include <windows.h>
#include "../include/HotkeyListener.h"
#include "../include/TextReplacer.h"

#define WM_TRAYICON (WM_APP + 1)
#define ID_EXIT 1001

NOTIFYICONDATA nid = { 0 };
HWND hWnd;
bool running = true;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    case WM_TRAYICON: {
        if (lParam == WM_RBUTTONDOWN || lParam == WM_LBUTTONDOWN)
        {
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
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_EXIT) {
            running = false;
            PostQuitMessage(0);
        }
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DummyWindowClass";
    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, "DummyWindowClass", "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = (HICON)LoadImage(hInstance, "kurva.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    nid.uFlags |= NIF_ICON;
    strcpy_s(nid.szTip, "Kurva Switcher");

    Shell_NotifyIcon(NIM_ADD, &nid);

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

    Shell_NotifyIcon(NIM_DELETE, &nid);

    return (int)msg.wParam;
}
