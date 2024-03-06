#include "../include/HotkeyListener.h"
#include <iostream>

HotkeyListener::HotkeyListener() : hotkeyId1(1), hotkeyId2(2), registered1(false), registered2(false) {}

HotkeyListener::~HotkeyListener() {
    if (registered1) {
        unregisterHotkey(hotkeyId1);
    }
    if (registered2) {
        unregisterHotkey(hotkeyId2);
    }
}

bool HotkeyListener::initialize() {
    bool success = true;

    if (RegisterHotKey(NULL, hotkeyId1, MOD_SHIFT, VK_PAUSE)) {
        registered1 = true;
    }
    else {
        std::cerr << "Unable to register Shift+Pause hotkey." << std::endl;
        success = false;
    }

    if (RegisterHotKey(NULL, hotkeyId2, 0, VK_PAUSE)) {
        registered2 = true;
    }
    else {
        std::cerr << "Unable to register Pause hotkey." << std::endl;
        success = false;
    }

    return success;
}

bool HotkeyListener::isHotkeyPressed(MSG& msg) const {
    return (msg.message == WM_HOTKEY &&
        (msg.wParam == hotkeyId1 || msg.wParam == hotkeyId2));
}

void HotkeyListener::unregisterHotkey(int hotkeyId) const {
    UnregisterHotKey(NULL, hotkeyId);
}
