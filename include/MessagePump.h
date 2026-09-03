#pragma once

#include <windows.h>

#include <chrono>
#include <concepts>

namespace kurva {

// Waiting helpers for the UI thread.
//
// The thread that runs a conversion also owns the windows that must keep answering
// messages while the conversion waits: the clipboard owner (WM_RENDERFORMAT), the
// tray icon and the hotkeys. Blocking that thread with Sleep() would deadlock the very
// application we are waiting for, so every wait pumps messages instead.
class MessagePump {
public:
    // Dispatches everything currently queued. Returns false once WM_QUIT has been seen;
    // the quit is remembered and can be re-posted with repostQuitIfPending().
    static bool drain();

    // Pumps messages for the given duration.
    static void pumpFor(std::chrono::milliseconds duration);

    // Pumps messages until predicate() returns true or the timeout expires.
    // Returns the last value of predicate().
    template <std::predicate Predicate>
    static bool waitUntil(Predicate predicate,
                          std::chrono::milliseconds timeout,
                          std::chrono::milliseconds pollInterval = std::chrono::milliseconds(5)) {
        using Clock = std::chrono::steady_clock;
        const auto deadline = Clock::now() + timeout;
        for (;;) {
            if (!drain()) {
                return predicate();
            }
            if (predicate()) {
                return true;
            }
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
            if (remaining <= std::chrono::milliseconds::zero()) {
                return false;
            }
            sleepUntilMessage(remaining < pollInterval ? remaining : pollInterval);
        }
    }

    static bool quitPending() noexcept { return quitPending_; }
    static void repostQuitIfPending();

private:
    static void sleepUntilMessage(std::chrono::milliseconds duration);

    static inline bool quitPending_ = false;
    static inline int quitCode_ = 0;
};

}  // namespace kurva
