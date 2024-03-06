#ifndef HOTKEYLISTENER_H
#define HOTKEYLISTENER_H

#include <windows.h>

class HotkeyListener {
public:
    HotkeyListener();
    ~HotkeyListener();

    bool initialize();
    void unregisterHotkey(int hotkeyId) const;
    bool isHotkeyPressed(MSG& msg) const;

private:
    int hotkeyId1;
    int hotkeyId2;
    bool registered1;
    bool registered2;
};

#endif // HOTKEYLISTENER_H
