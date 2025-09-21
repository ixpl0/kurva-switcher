#include "../include/HotkeyListener.h"
#include <windows.h>
#include <algorithm>

HotkeyListener::HotkeyListener()
    : hotkeyIds{1, 2}, registered{false, false} {
}

HotkeyListener::~HotkeyListener() {
    unregisterAllHotkeys();
}

bool HotkeyListener::initialize() {
    bool success = true;
    success &= registerHotkey(hotkeyIds[0], MOD_SHIFT, VK_PAUSE, "Shift+Pause");
    success &= registerHotkey(hotkeyIds[1], 0, VK_PAUSE, "Pause");
    return success;
}

bool HotkeyListener::isHotkeyPressed(const MSG& msg) const {
    if (msg.message == WM_HOTKEY) {
        const auto id = static_cast<int>(msg.wParam);
        return std::find(hotkeyIds.begin(), hotkeyIds.end(), id) != hotkeyIds.end();
    }
    return false;
}

void HotkeyListener::unregisterAllHotkeys() {
    for (size_t i = 0; i < HOTKEY_COUNT; ++i) {
        if (registered[i]) {
            UnregisterHotKey(nullptr, hotkeyIds[i]);
            registered[i] = false;
        }
    }
}

bool HotkeyListener::registerHotkey(int hotkeyId, UINT fsModifiers, UINT vk, const std::string& hotkeyName) {
    if (RegisterHotKey(nullptr, hotkeyId, fsModifiers, vk)) {
        for (size_t i = 0; i < HOTKEY_COUNT; ++i) {
            if (hotkeyIds[i] == hotkeyId) {
                registered[i] = true;
                break;
            }
        }
        return true;
    }
    return false;
}

