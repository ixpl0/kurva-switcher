#ifndef HOTKEYLISTENER_H
#define HOTKEYLISTENER_H

#include <windows.h>
#include <string>

class HotkeyListener {
public:
    HotkeyListener();
    ~HotkeyListener();

    bool initialize();
    void unregisterHotkey(int hotkeyId) const;
    bool isHotkeyPressed(MSG& msg) const;

private:
    void unregisterAllHotkeys();
    bool registerHotkey(int hotkeyId, UINT fsModifiers, UINT vk, const std::string& hotkeyName);

    int hotkeyId1;
    int hotkeyId2;
    bool registered1;
    bool registered2;
};

#endif // HOTKEYLISTENER_H
