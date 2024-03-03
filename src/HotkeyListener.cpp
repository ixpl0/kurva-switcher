#include "../include/HotkeyListener.h"
#include <iostream>

HotkeyListener::HotkeyListener() : hotkeyId(1), registered(false) {}

HotkeyListener::~HotkeyListener() {
    if (registered) {
        unregisterHotkey();
    }
}

bool HotkeyListener::initialize() {
    if (RegisterHotKey(NULL, hotkeyId, 0, VK_PAUSE)) {
        registered = true;
        return true;
    }

    std::cerr << "Unable to register hotkey." << std::endl;

    return false;
}

bool HotkeyListener::isHotkeyPressed(MSG& msg) const {
    if (msg.message == WM_HOTKEY && msg.wParam == hotkeyId) {
        return true;
    }
    return false;
}

void HotkeyListener::unregisterHotkey() const {
    UnregisterHotKey(NULL, hotkeyId);
}
