#include "Input.h"

#include <array>
#include <vector>

#include "Log.h"

namespace kurva::input {

namespace {

bool isExtendedKey(WORD virtualKey) {
    switch (virtualKey) {
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_LWIN:
    case VK_RWIN:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
        return true;
    default:
        return false;
    }
}

INPUT keyEvent(WORD virtualKey, bool release) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = (release ? KEYEVENTF_KEYUP : 0u) | (isExtendedKey(virtualKey) ? KEYEVENTF_EXTENDEDKEY : 0u);
    input.ki.dwExtraInfo = kInjectedTag;
    return input;
}

bool send(std::vector<INPUT>& inputs) {
    if (inputs.empty()) {
        return true;
    }
    const UINT count = static_cast<UINT>(inputs.size());
    const UINT sent = SendInput(count, inputs.data(), sizeof(INPUT));
    if (sent != count) {
        log::info(L"input: SendInput sent {} of {} events (error {})", sent, count, GetLastError());
        return false;
    }
    return true;
}

}  // namespace

int releaseModifiers() {
    constexpr std::array<WORD, 8> modifiers{
        VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU, VK_LWIN, VK_RWIN,
    };
    std::vector<INPUT> inputs;
    for (const WORD virtualKey : modifiers) {
        if (GetAsyncKeyState(virtualKey) & 0x8000) {
            inputs.push_back(keyEvent(virtualKey, true));
        }
    }
    if (inputs.empty()) {
        return 0;
    }
    send(inputs);
    return static_cast<int>(inputs.size());
}

bool sendCtrlChord(WORD virtualKey) {
    std::vector<INPUT> inputs{
        keyEvent(VK_LCONTROL, false),
        keyEvent(virtualKey, false),
        keyEvent(virtualKey, true),
        keyEvent(VK_LCONTROL, true),
    };
    return send(inputs);
}

}  // namespace kurva::input
