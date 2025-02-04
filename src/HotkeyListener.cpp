#include "../include/HotkeyListener.h"
#include <windows.h>

HotkeyListener::HotkeyListener()
    : hotkeyId1(1), hotkeyId2(2), registered1(false), registered2(false) {
}

HotkeyListener::~HotkeyListener() {
    unregisterAllHotkeys();
}

bool HotkeyListener::initialize() {
    bool success = true;

    success &= registerHotkey(hotkeyId1, MOD_SHIFT, VK_PAUSE, "Shift+Pause");
    success &= registerHotkey(hotkeyId2, 0, VK_PAUSE, "Pause");

    return success;
}

bool HotkeyListener::isHotkeyPressed(MSG& msg) const {
    if (msg.message == WM_HOTKEY) {
        return (msg.wParam == hotkeyId1 || msg.wParam == hotkeyId2);
    }

    return false;
}

void HotkeyListener::unregisterHotkey(int hotkeyId) const {
    UnregisterHotKey(NULL, hotkeyId);
}

void HotkeyListener::unregisterAllHotkeys() {
    if (registered1) {
        unregisterHotkey(hotkeyId1);
        registered1 = false;
    }

    if (registered2) {
        unregisterHotkey(hotkeyId2);
        registered2 = false;
    }
}

bool HotkeyListener::registerHotkey(int hotkeyId, UINT fsModifiers, UINT vk, const std::string& hotkeyName) {
    if (RegisterHotKey(NULL, hotkeyId, fsModifiers, vk)) {
        if (hotkeyId == hotkeyId1) {
            registered1 = true;
        }

        if (hotkeyId == hotkeyId2) {
            registered2 = true;
        }

        return true;
    }
    else {
        return false;
    }
}

