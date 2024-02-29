#ifndef HOTKEYLISTENER_H
#define HOTKEYLISTENER_H

#include <windows.h>

class HotkeyListener {
public:
    HotkeyListener();
    ~HotkeyListener();

    bool initialize();
    bool isHotkeyPressed(MSG& msg) const;

private:
    int hotkeyId;
    bool registered;
    void unregisterHotkey() const;
};

#endif // HOTKEYLISTENER_H
