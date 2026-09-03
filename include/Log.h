#pragma once

#include <windows.h>

#include <format>
#include <string>
#include <string_view>
#include <utility>

// Diagnostics go to the debugger output stream: watch them with DebugView
// (Sysinternals) or the Visual Studio "Output" window while reproducing a problem.
//
// Never log clipboard or selection *content* here: it may be a password.
namespace kurva::log {

inline void line(std::wstring_view message) {
    std::wstring buffer;
    buffer.reserve(message.size() + 10);
    buffer.append(L"[kurva] ").append(message).append(L"\n");
    OutputDebugStringW(buffer.c_str());
}

template <typename... Args>
void info(std::wformat_string<Args...> format, Args&&... args) {
    line(std::format(format, std::forward<Args>(args)...));
}

}  // namespace kurva::log
