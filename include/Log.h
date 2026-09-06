#pragma once

#include <windows.h>

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Diagnostics go two ways: to the debugger output stream, which DebugView (Sysinternals) and
// the Visual Studio "Output" window show, and to a buffer of the most recent lines that the
// "Log..." window of the tray menu shows.
//
// Never log clipboard or selection *content* here: it may be a password.
namespace kurva::log {

void line(std::wstring_view message);

template <typename... Args>
void info(std::wformat_string<Args...> format, Args&&... args) {
    line(std::format(format, std::forward<Args>(args)...));
}

// How many lines the buffer keeps.
inline constexpr size_t kRecentCapacity = 500;

// The buffered lines, oldest first, each prefixed with the time of day. `total` counts every
// line logged since the program started, so a reader can tell which lines it has already seen.
struct Recent {
    unsigned long long total = 0;
    std::vector<std::wstring> lines;
};

[[nodiscard]] Recent recent();

// Forgets the buffered lines; `total` keeps counting.
void clear();

}  // namespace kurva::log
