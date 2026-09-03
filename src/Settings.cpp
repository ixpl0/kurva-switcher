#include "Settings.h"

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Log.h"

namespace kurva::settings {

namespace {

constexpr wchar_t kSettingsKey[] = L"Software\\kurva-switcher";
constexpr wchar_t kSwitchLayoutValue[] = L"SwitchLayoutAfterConversion";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"kurva-switcher";

std::optional<DWORD> readDword(const wchar_t* key, const wchar_t* value) {
    DWORD data = 0;
    DWORD size = sizeof(data);
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &data, &size) == ERROR_SUCCESS) {
        return data;
    }
    return std::nullopt;
}

std::optional<std::wstring> readString(const wchar_t* key, const wchar_t* value) {
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    std::wstring data(bytes / sizeof(wchar_t) + 1, L'\0');
    bytes = static_cast<DWORD>(data.size() * sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, data.data(), &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    data.resize(wcsnlen(data.c_str(), data.size()));
    return data;
}

// "C:\path\app.exe" or "C:\path\app.exe" --args or C:\path\app.exe  ->  C:\path\app.exe
std::wstring executableOfCommand(std::wstring_view command) {
    while (!command.empty() && (command.front() == L' ' || command.front() == L'\t')) {
        command.remove_prefix(1);
    }
    if (!command.empty() && command.front() == L'"') {
        command.remove_prefix(1);
        const size_t closing = command.find(L'"');
        return std::wstring(command.substr(0, closing));
    }
    while (!command.empty() && (command.back() == L' ' || command.back() == L'\t')) {
        command.remove_suffix(1);
    }
    return std::wstring(command);
}

bool samePath(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size() || left.size() > static_cast<size_t>(INT32_MAX)) {
        return false;
    }
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool startsWithPath(std::wstring_view path, std::wstring_view prefix) {
    return path.size() >= prefix.size() && samePath(path.substr(0, prefix.size()), prefix);
}

bool fileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

}  // namespace

bool switchLayoutAfterConversion() {
    return readDword(kSettingsKey, kSwitchLayoutValue).value_or(1) != 0;
}

void setSwitchLayoutAfterConversion(bool enabled) {
    const DWORD value = enabled ? 1 : 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, kSettingsKey, kSwitchLayoutValue, REG_DWORD, &value, sizeof(value));
}

std::wstring executablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            return std::wstring(buffer.data(), length);
        }
        if (buffer.size() >= 32768) {
            return {};
        }
        buffer.resize(buffer.size() * 2);  // Truncated: the path is longer than MAX_PATH.
    }
}

bool isTemporaryLocation(const std::wstring& path) {
    wchar_t temp[MAX_PATH + 1];
    const DWORD length = GetTempPathW(MAX_PATH + 1, temp);
    if (length == 0 || length > MAX_PATH) {
        return false;
    }
    return startsWithPath(path, std::wstring_view(temp, length));
}

Autostart autostart() {
    Autostart result;
    const std::optional<std::wstring> command = readString(kRunKey, kRunValue);
    if (!command) {
        return result;
    }
    result.registeredPath = executableOfCommand(*command);
    result.state = samePath(result.registeredPath, executablePath()) ? Autostart::State::ThisExecutable
                                                                     : Autostart::State::OtherExecutable;
    return result;
}

bool runAtStartup() {
    return autostart().state == Autostart::State::ThisExecutable;
}

bool setRunAtStartup(bool enabled) {
    if (!enabled) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    const std::wstring path = executablePath();
    if (path.empty()) {
        return false;
    }
    const std::wstring command = L"\"" + path + L"\"";
    const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    const LSTATUS status = RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ, command.c_str(), bytes);
    if (status != ERROR_SUCCESS) {
        log::info(L"autostart: cannot write the Run entry (error {})", status);
        return false;
    }
    log::info(L"autostart: Run entry now starts {}", command);
    return true;
}

bool repairAutostart() {
    const Autostart current = autostart();
    if (current.state != Autostart::State::OtherExecutable) {
        return false;
    }
    if (fileExists(current.registeredPath)) {
        log::info(L"autostart: the Run entry starts another copy, {}; leaving it alone", current.registeredPath);
        return false;
    }
    log::info(L"autostart: the Run entry points to a missing file, {}; repointing it", current.registeredPath);
    return setRunAtStartup(true);
}

}  // namespace kurva::settings
