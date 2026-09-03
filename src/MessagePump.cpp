#include "MessagePump.h"

namespace kurva {

bool MessagePump::drain() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            quitPending_ = true;
            quitCode_ = static_cast<int>(msg.wParam);
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !quitPending_;
}

void MessagePump::pumpFor(std::chrono::milliseconds duration) {
    waitUntil([] { return false; }, duration, duration);
}

void MessagePump::repostQuitIfPending() {
    if (quitPending_) {
        quitPending_ = false;
        PostQuitMessage(quitCode_);
    }
}

void MessagePump::sleepUntilMessage(std::chrono::milliseconds duration) {
    const auto count = duration.count();
    const DWORD milliseconds = count <= 0 ? 1u : static_cast<DWORD>(count);
    // MWMO_INPUTAVAILABLE: return at once if a message is already queued, even one that
    // an earlier PeekMessage has looked at.
    MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
}

}  // namespace kurva
