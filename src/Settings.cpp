#include "Settings.h"

#include <windows.h>

#include <shlobj.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Log.h"

namespace kurva::settings {

namespace {

constexpr wchar_t kFileName[] = L"kurva-switcher.ini";
constexpr wchar_t kSection[] = L"kurva-switcher";
constexpr wchar_t kEnabledKey[] = L"Enabled";
constexpr wchar_t kSwitchLayoutKey[] = L"SwitchLayoutAfterConversion";
constexpr const wchar_t* kHotkeyKeys[kHotkeySlots] = {L"Hotkey", L"Hotkey2"};
constexpr wchar_t kWelcomeShownKey[] = L"WelcomeShown";

constexpr std::array<Hotkey, kHotkeySlots> kDefaultHotkeys{{
    kDefaultHotkey,
    Hotkey{.modifiers = MOD_SHIFT, .virtualKey = VK_PAUSE},
}};

// Builds before 0.4 kept their settings here.
constexpr wchar_t kLegacyKey[] = L"Software\\kurva-switcher";
constexpr wchar_t kLegacySwitchLayoutValue[] = L"SwitchLayoutAfterConversion";
constexpr wchar_t kLegacyHotkeyValue[] = L"Hotkey";

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"kurva-switcher";

std::wstring& filePathStorage() {
    static std::wstring path;
    return path;
}

bool sameText(std::wstring_view left, std::wstring_view right) {
    if (left.size() != right.size() || left.size() > static_cast<size_t>(INT32_MAX)) {
        return false;
    }
    return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool startsWithPath(std::wstring_view path, std::wstring_view prefix) {
    return path.size() >= prefix.size() && sameText(path.substr(0, prefix.size()), prefix);
}

bool fileExists(const std::wstring& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// The folder of a file, trailing backslash included: C:\path\app.exe gives "C:\path\".
std::wstring directoryOf(std::wstring_view path) {
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring_view::npos) {
        return {};
    }
    return std::wstring(path.substr(0, slash + 1));
}

// Asks for the right to add files to the directory; nothing is created. Works with NTFS
// permissions and network shares alike.
bool directoryWritable(const std::wstring& directory) {
    const HANDLE handle = CreateFileW(directory.c_str(), FILE_ADD_FILE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(handle);
    return true;
}

// %LOCALAPPDATA%\kurva-switcher\kurva-switcher.ini, creating the folder. Empty on failure.
std::wstring localAppDataFile() {
    PWSTR folder = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder)) || !folder) {
        return {};
    }
    std::wstring directory = folder;
    CoTaskMemFree(folder);
    directory += L"\\kurva-switcher";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return {};
    }
    return directory + L"\\" + kFileName;
}

// --- The file ---------------------------------------------------------------------------

// The profile API answers with the default for a missing key; a sentinel default tells a
// missing key apart from an empty value.
std::optional<std::wstring> readValue(const wchar_t* key) {
    constexpr wchar_t kMissing[] = L"\x01";
    wchar_t buffer[1024];
    const DWORD length = GetPrivateProfileStringW(kSection, key, kMissing, buffer,
                                                  static_cast<DWORD>(ARRAYSIZE(buffer)), filePathStorage().c_str());
    std::wstring value(buffer, length);
    if (value == kMissing) {
        return std::nullopt;
    }
    return value;
}

bool writeValue(const wchar_t* key, const std::wstring& value) {
    if (WritePrivateProfileStringW(kSection, key, value.c_str(), filePathStorage().c_str())) {
        return true;
    }
    log::info(L"settings: cannot write {} to {} (error {})", key, filePathStorage(), GetLastError());
    return false;
}

std::optional<bool> readBool(const wchar_t* key) {
    const std::optional<std::wstring> value = readValue(key);
    if (!value) {
        return std::nullopt;
    }
    if (*value == L"1" || sameText(*value, L"true") || sameText(*value, L"yes") || sameText(*value, L"on")) {
        return true;
    }
    if (*value == L"0" || sameText(*value, L"false") || sameText(*value, L"no") || sameText(*value, L"off")) {
        return false;
    }
    return std::nullopt;
}

bool writeBool(const wchar_t* key, bool value) {
    return writeValue(key, value ? L"1" : L"0");
}

std::optional<Hotkey> readHotkey(size_t slot) {
    const std::optional<std::wstring> text = readValue(kHotkeyKeys[slot]);
    if (!text) {
        return std::nullopt;
    }
    const std::optional<Hotkey> parsed = Hotkey::parse(*text);
    if (!parsed) {
        log::info(L"settings: {}=\"{}\" is not a hotkey", kHotkeyKeys[slot], *text);
    }
    return parsed;
}

// Writes the defaults for whatever is missing (or unreadable), so that the file lists every
// setting there is.
void fillDefaults() {
    if (!readBool(kEnabledKey)) {
        writeBool(kEnabledKey, true);
    }
    if (!readBool(kSwitchLayoutKey)) {
        writeBool(kSwitchLayoutKey, true);
    }
    for (size_t slot = 0; slot < kHotkeySlots; ++slot) {
        if (!readHotkey(slot)) {
            writeValue(kHotkeyKeys[slot], kDefaultHotkeys[slot].serialize());
        }
    }
    if (!readBool(kWelcomeShownKey)) {
        writeBool(kWelcomeShownKey, false);
    }
}

// --- The registry of older builds ------------------------------------------------------

std::optional<DWORD> readRegistryDword(const wchar_t* key, const wchar_t* value) {
    DWORD data = 0;
    DWORD size = sizeof(data);
    if (RegGetValueW(HKEY_CURRENT_USER, key, value, RRF_RT_REG_DWORD, nullptr, &data, &size) == ERROR_SUCCESS) {
        return data;
    }
    return std::nullopt;
}

std::optional<std::wstring> readRegistryString(const wchar_t* key, const wchar_t* value) {
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

// Those builds packed the hotkey into one DWORD: the virtual key in the low byte, the MOD_*
// flags in the next one, and the extended-key bit above them.
Hotkey unpackLegacyHotkey(DWORD value) {
    constexpr DWORD kExtendedKeyBit = 1u << 16;
    return Hotkey{.modifiers = (value >> 8) & (MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN),
                  .virtualKey = value & 0xFF,
                  .extendedKey = (value & kExtendedKeyBit) != 0};
}

void migrateFromRegistry() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLegacyKey, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return;
    }
    RegCloseKey(key);

    bool written = true;
    if (const std::optional<DWORD> switchLayout = readRegistryDword(kLegacyKey, kLegacySwitchLayoutValue)) {
        written = writeBool(kSwitchLayoutKey, *switchLayout != 0) && written;
    }
    if (const std::optional<DWORD> packed = readRegistryDword(kLegacyKey, kLegacyHotkeyValue)) {
        const Hotkey first = unpackLegacyHotkey(*packed);
        if (!first.empty()) {
            written = writeValue(kHotkeyKeys[0], first.serialize()) && written;
            // Those builds also registered a plain key with Shift; keep that as the second hotkey.
            Hotkey second;
            if (first.modifiers == 0) {
                second = first;
                second.modifiers = MOD_SHIFT;
            }
            written = writeValue(kHotkeyKeys[1], second.serialize()) && written;
        }
    }
    written = writeBool(kWelcomeShownKey, true) && written;  // Not a first start.
    if (!written) {
        log::info(L"settings: the registry settings could not be moved to the file; leaving them");
        return;
    }
    const LSTATUS status = RegDeleteKeyW(HKEY_CURRENT_USER, kLegacyKey);
    log::info(L"settings: moved the registry settings to the file{}",
              status == ERROR_SUCCESS ? L"" : L" (the registry key could not be deleted)");
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

}  // namespace

void initialize() {
    std::wstring& path = filePathStorage();
    const std::wstring directory = directoryOf(executablePath());
    path = directory + kFileName;
    if (directory.empty() || (!fileExists(path) && !directoryWritable(directory))) {
        const std::wstring fallback = localAppDataFile();
        if (!fallback.empty()) {
            log::info(L"settings: {} is not writable, using {}", directory, fallback);
            path = fallback;
        }
    }
    log::info(L"settings: {}", path);
    migrateFromRegistry();
    fillDefaults();
}

std::wstring filePath() {
    return filePathStorage();
}

bool enabled() {
    return readBool(kEnabledKey).value_or(true);
}

void setEnabled(bool value) {
    writeBool(kEnabledKey, value);
}

bool switchLayoutAfterConversion() {
    return readBool(kSwitchLayoutKey).value_or(true);
}

void setSwitchLayoutAfterConversion(bool value) {
    writeBool(kSwitchLayoutKey, value);
}

Hotkey hotkey(size_t slot) {
    if (slot >= kHotkeySlots) {
        return {};
    }
    return readHotkey(slot).value_or(kDefaultHotkeys[slot]);
}

void setHotkey(size_t slot, const Hotkey& combination) {
    if (slot < kHotkeySlots) {
        writeValue(kHotkeyKeys[slot], combination.serialize());
    }
}

bool welcomeShown() {
    return readBool(kWelcomeShownKey).value_or(false);
}

void setWelcomeShown(bool shown) {
    writeBool(kWelcomeShownKey, shown);
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
    const std::optional<std::wstring> command = readRegistryString(kRunKey, kRunValue);
    if (!command) {
        return result;
    }
    result.registeredPath = executableOfCommand(*command);
    result.state = sameText(result.registeredPath, executablePath()) ? Autostart::State::ThisExecutable
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
