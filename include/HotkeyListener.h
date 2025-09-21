#ifndef HOTKEYLISTENER_H
#define HOTKEYLISTENER_H

#include <windows.h>
#include <string>
#include <array>

class HotkeyListener {
public:
    HotkeyListener();
    ~HotkeyListener();

    HotkeyListener(const HotkeyListener&) = delete;
    HotkeyListener& operator=(const HotkeyListener&) = delete;
    HotkeyListener(HotkeyListener&&) = delete;
    HotkeyListener& operator=(HotkeyListener&&) = delete;

    bool initialize();
    bool isHotkeyPressed(const MSG& msg) const;

private:
    void unregisterAllHotkeys();
    bool registerHotkey(int hotkeyId, UINT fsModifiers, UINT vk, const std::string& hotkeyName);

    static constexpr int HOTKEY_COUNT = 2;
    std::array<int, HOTKEY_COUNT> hotkeyIds;
    std::array<bool, HOTKEY_COUNT> registered;
};

#endif // HOTKEYLISTENER_H
