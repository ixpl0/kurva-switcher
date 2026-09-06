#include "Log.h"

#include <deque>
#include <mutex>

namespace kurva::log {

namespace {

struct Buffer {
    std::mutex mutex;
    std::deque<std::wstring> lines;
    unsigned long long total = 0;
};

// Built on first use: logging may start before other globals are ready.
Buffer& buffer() {
    static Buffer instance;
    return instance;
}

std::wstring timeOfDay() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    return std::format(L"{:02}:{:02}:{:02}.{:03}", now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
}

}  // namespace

void line(std::wstring_view message) {
    std::wstring debug;
    debug.reserve(message.size() + 10);
    debug.append(L"[kurva] ").append(message).append(L"\n");
    OutputDebugStringW(debug.c_str());

    std::wstring stamped = timeOfDay();
    stamped.append(L"  ").append(message);

    Buffer& state = buffer();
    const std::scoped_lock lock(state.mutex);
    state.lines.push_back(std::move(stamped));
    if (state.lines.size() > kRecentCapacity) {
        state.lines.pop_front();
    }
    ++state.total;
}

Recent recent() {
    Buffer& state = buffer();
    const std::scoped_lock lock(state.mutex);
    return Recent{.total = state.total, .lines = std::vector<std::wstring>(state.lines.begin(), state.lines.end())};
}

void clear() {
    Buffer& state = buffer();
    const std::scoped_lock lock(state.mutex);
    state.lines.clear();
}

}  // namespace kurva::log
