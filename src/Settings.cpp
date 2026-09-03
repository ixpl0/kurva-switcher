#include "Settings.h"

#include <windows.h>

#include <optional>
#include <string>

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

}  // namespace

bool switchLayoutAfterConversion() {
    return readDword(kSettingsKey, kSwitchLayoutValue).value_or(1) != 0;
}

void setSwitchLayoutAfterConversion(bool enabled) {
    const DWORD value = enabled ? 1 : 0;
    RegSetKeyValueW(HKEY_CURRENT_USER, kSettingsKey, kSwitchLayoutValue, REG_DWORD, &value, sizeof(value));
}

bool runAtStartup() {
    DWORD size = 0;
    return RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, &size) == ERROR_SUCCESS;
}

bool setRunAtStartup(bool enabled) {
    if (!enabled) {
        const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    const std::wstring command = L"\"" + std::wstring(path, length) + L"\"";
    const DWORD bytes = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
    return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, REG_SZ, command.c_str(), bytes) == ERROR_SUCCESS;
}

}  // namespace kurva::settings
