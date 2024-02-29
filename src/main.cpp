#include <windows.h>
#include "../include/HotkeyListener.h"
#include "../include/TextReplacer.h"

int main() {
    HotkeyListener hotkeyListener;

    if (!hotkeyListener.initialize()) {
        return 1;
    }

    TextReplacer textReplacer;
    MSG msg = { 0 };

    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        if (hotkeyListener.isHotkeyPressed(msg)) {
            textReplacer.replaceSelectedText();
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
