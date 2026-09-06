#include <windows.h>

#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "AboutDialog.h"
#include "DarkMode.h"
#include "Dialogs.h"
#include "Hotkey.h"
#include "HotkeyDialog.h"
#include "Log.h"
#include "LogDialog.h"
#include "Settings.h"
#include "TextSwitcher.h"
#include "TrayIcon.h"

// Version 6 of the common controls: without it the dialogs are drawn in the classic
// Windows 2000 style instead of the current visual style, and SysLink does not exist.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using namespace kurva;

constexpr wchar_t kWindowClass[] = L"KurvaSwitcher.App";
constexpr wchar_t kMutexName[] = L"Local\\kurva-switcher.single-instance";
constexpr wchar_t kAppTitle[] = L"kurva-switcher";

constexpr UINT WM_TRAYICON = WM_APP + 1;

enum MenuItem : UINT {
    kMenuEnabled = 1000,
    kMenuHotkey,
    kMenuSecondHotkey,
    kMenuSwitchLayout,
    kMenuRunAtStartup,
    kMenuLog,
    kMenuAbout,
    kMenuExit,
};

// Hotkey slot n is registered with Windows under the id kFirstHotkeyId + n.
constexpr int kFirstHotkeyId = 1;

// A combination held by another program is tried again this often, so that it becomes ours
// as soon as that program quits, with no restart.
constexpr UINT_PTR kRetryTimerId = 1;
constexpr UINT kRetryIntervalMs = 3000;

UINT checkMark(bool checked) {
    return checked ? static_cast<UINT>(MF_CHECKED) : static_cast<UINT>(MF_UNCHECKED);
}

std::wstring join(const std::vector<std::wstring>& items, const wchar_t* separator) {
    std::wstring text;
    for (size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            text += separator;
        }
        text += items[index];
    }
    return text;
}

class App {
public:
    explicit App(HINSTANCE instance) : instance_(instance) {}
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool initialize();
    int run();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    bool createWindow();
    // Registers every configured combination; returns the names of those that could not be.
    std::vector<std::wstring> registerHotkeys();
    bool registerHotkey(size_t slot, bool quiet) const;
    void unregisterHotkeys();
    void retryHotkeys();
    void announceConflict(const std::vector<std::wstring>& failed);
    void welcome();
    [[nodiscard]] std::wstring status() const;
    void updateStatus();
    [[nodiscard]] std::wstring hotkeyLabel(size_t slot) const;
    void setEnabled(bool enabled);
    void changeHotkey(size_t slot);
    void showTrayMenu();
    void toggleRunAtStartup();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    bool classRegistered_ = false;
    UINT taskbarCreatedMessage_ = 0;
    bool menuOpen_ = false;
    bool enabled_ = true;
    std::array<Hotkey, settings::kHotkeySlots> hotkeys_{};
    std::array<bool, settings::kHotkeySlots> registered_{};
    bool retrying_ = false;  // The retry timer is running.
    std::optional<TrayIcon> tray_;
    std::optional<TextSwitcher> switcher_;
};

App::~App() {
    unregisterHotkeys();
    tray_.reset();
    switcher_.reset();
    if (window_) {
        DestroyWindow(window_);
    }
    if (classRegistered_) {
        UnregisterClassW(kWindowClass, instance_);
    }
}

bool App::initialize() {
    darkmode::allowDarkMenus();  // Before any window or menu exists.
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;  // SysLink, for the About box.
    InitCommonControlsEx(&controls);
    settings::initialize();

    if (!createWindow()) {
        MessageBoxW(nullptr, L"Failed to create the application window.", kAppTitle, MB_ICONERROR);
        return false;
    }

    switcher_.emplace(instance_);
    if (!switcher_->isReady()) {
        MessageBoxW(nullptr, L"Failed to create the clipboard window.", kAppTitle, MB_ICONERROR);
        return false;
    }
    switcher_->setSwitchKeyboardLayout(settings::switchLayoutAfterConversion());

    enabled_ = settings::enabled();
    for (size_t slot = 0; slot < settings::kHotkeySlots; ++slot) {
        hotkeys_[slot] = settings::hotkey(slot);
    }

    tray_.emplace(instance_, window_, WM_TRAYICON);
    tray_->setDisabled(!enabled_);
    tray_->add();

    const bool firstStart = !settings::welcomeShown();
    std::vector<std::wstring> failed;
    if (enabled_) {
        failed = registerHotkeys();
    }
    updateStatus();
    if (firstStart) {
        welcome();
        settings::setWelcomeShown(true);
    }
    if (!failed.empty()) {
        announceConflict(failed);  // After the welcome: if only one notification stays, this one.
    }
    settings::repairAutostart();  // The build was moved or replaced since autostart was enabled.
    return true;
}

int App::run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

bool App::createWindow() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &App::windowProc;
    windowClass.hInstance = instance_;
    windowClass.lpszClassName = kWindowClass;
    classRegistered_ = RegisterClassExW(&windowClass) != 0;
    if (!classRegistered_) {
        return false;
    }

    // A hidden top-level window rather than a message-only one: only top-level windows
    // receive the broadcast that Explorer sends after it restarts (TaskbarCreated), and
    // only a real window can be brought to the foreground so the tray menu closes properly.
    const HWND created = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, kAppTitle, WS_OVERLAPPED, 0, 0, 0, 0,
                                         nullptr, nullptr, instance_, this);
    if (!created) {
        return false;
    }
    window_ = created;
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    return true;
}

std::vector<std::wstring> App::registerHotkeys() {
    unregisterHotkeys();
    std::vector<std::wstring> failed;
    for (size_t slot = 0; slot < settings::kHotkeySlots; ++slot) {
        if (hotkeys_[slot].empty()) {
            continue;
        }
        registered_[slot] = registerHotkey(slot, false);
        if (!registered_[slot]) {
            failed.push_back(hotkeys_[slot].name());
        }
    }
    if (!failed.empty()) {
        retrying_ = SetTimer(window_, kRetryTimerId, kRetryIntervalMs, nullptr) != 0;
    }
    return failed;
}

bool App::registerHotkey(size_t slot, bool quiet) const {
    const Hotkey& hotkey = hotkeys_[slot];
    const int id = kFirstHotkeyId + static_cast<int>(slot);
    // MOD_NOREPEAT keeps a held-down key from firing the conversion again and again.
    bool ok = RegisterHotKey(window_, id, hotkey.modifiers | MOD_NOREPEAT, hotkey.virtualKey) != FALSE;
    if (!ok) {
        ok = RegisterHotKey(window_, id, hotkey.modifiers, hotkey.virtualKey) != FALSE;
    }
    if (!ok && !quiet) {
        log::info(L"hotkey: cannot register {} (error {})", hotkey.name(), GetLastError());
    }
    return ok;
}

void App::unregisterHotkeys() {
    if (retrying_) {
        KillTimer(window_, kRetryTimerId);
        retrying_ = false;
    }
    for (size_t slot = 0; slot < settings::kHotkeySlots; ++slot) {
        if (registered_[slot]) {
            UnregisterHotKey(window_, kFirstHotkeyId + static_cast<int>(slot));
            registered_[slot] = false;
        }
    }
}

// Timer: the combinations that were taken when they were last tried.
void App::retryHotkeys() {
    std::vector<std::wstring> gained;
    bool stillTaken = false;
    for (size_t slot = 0; slot < settings::kHotkeySlots; ++slot) {
        if (hotkeys_[slot].empty() || registered_[slot]) {
            continue;
        }
        registered_[slot] = registerHotkey(slot, true);
        if (registered_[slot]) {
            gained.push_back(hotkeys_[slot].name());
        } else {
            stillTaken = true;
        }
    }
    if (!gained.empty()) {
        updateStatus();
        log::info(L"hotkey: {} registered after all", join(gained, L" and "));
        tray_->notify(kAppTitle,
                      join(gained, L" and ") + (gained.size() == 1 ? L" is" : L" are") +
                          L" no longer taken: converting the selected text works now.",
                      TrayIcon::Notice::Info);
    }
    if (!stillTaken && retrying_) {
        KillTimer(window_, kRetryTimerId);
        retrying_ = false;
    }
}

void App::announceConflict(const std::vector<std::wstring>& failed) {
    const bool one = failed.size() == 1;
    const std::wstring text = std::wstring(L"Could not register ") + (one ? L"the hotkey " : L"the hotkeys ") +
                              join(failed, L" and ") + L": another program (Punto Switcher, for example) uses " +
                              (one ? L"it" : L"them") +
                              L". Right-click the tray icon to choose a different combination; meanwhile it is "
                              L"retried every few seconds.";
    tray_->notify(L"kurva-switcher: hotkey conflict", text, TrayIcon::Notice::Warning);
}

// The first start: the program has no window, so say where it went and what it does.
void App::welcome() {
    std::vector<std::wstring> names;
    for (const Hotkey& hotkey : hotkeys_) {
        if (!hotkey.empty()) {
            names.push_back(hotkey.name());
        }
    }
    const std::wstring text =
        names.empty() ? std::wstring(L"Right-click the kurva-switcher icon in the system tray to choose a hotkey. Then select "
                                     L"text typed in the wrong layout and press it.")
                      : L"Select text typed in the wrong layout and press " + join(names, L" or ") +
                            L" to retype it. Right-click the kurva-switcher icon in the system tray for the settings.";
    tray_->notify(L"kurva-switcher is running", text, TrayIcon::Notice::Welcome);
}

// The second line of the tray tooltip.
std::wstring App::status() const {
    if (!enabled_) {
        return L"Disabled. Right-click to enable.";
    }
    std::vector<std::wstring> names;
    for (size_t slot = 0; slot < settings::kHotkeySlots; ++slot) {
        if (registered_[slot]) {
            names.push_back(hotkeys_[slot].name());
        }
    }
    if (names.empty()) {
        return L"No hotkey is registered. Right-click to choose one.";
    }
    return join(names, L" or ") + L" converts the selected text";
}

void App::updateStatus() {
    tray_->setStatus(status());
}

std::wstring App::hotkeyLabel(size_t slot) const {
    const Hotkey& hotkey = hotkeys_[slot];
    if (hotkey.empty()) {
        return L"none";
    }
    if (enabled_ && !registered_[slot]) {
        return hotkey.name() + L" (taken by another program)";
    }
    return hotkey.name();
}

void App::setEnabled(bool enabled) {
    enabled_ = enabled;
    settings::setEnabled(enabled);
    tray_->setDisabled(!enabled);
    std::vector<std::wstring> failed;
    if (enabled) {
        failed = registerHotkeys();
    } else {
        unregisterHotkeys();
    }
    updateStatus();
    log::info(L"hotkeys {}", enabled ? L"enabled" : L"disabled");
    if (!failed.empty()) {
        announceConflict(failed);
    }
}

void App::changeHotkey(size_t slot) {
    if (dialogs::focusOpen()) {
        return;
    }
    // While the dialog is open the current combinations must reach its field as ordinary keys.
    unregisterHotkeys();
    const size_t other = (slot + 1) % settings::kHotkeySlots;
    const bool allowEmpty = !hotkeys_[other].empty();
    std::wstring hint = slot == 0 ? L"Pause, F9... or a combination with Ctrl, Alt or Win."
                                  : L"Another combination for the same action.";
    if (allowEmpty) {
        hint += L" Backspace clears the field.";
    }
    const hotkeydialog::Request request{
        .current = hotkeys_[slot],
        .other = hotkeys_[other],
        .prompt = slot == 0 ? L"Press the key combination that converts the selected text:"
                            : L"Press the second key combination that converts the selected text:",
        .hint = hint.c_str(),
        .allowEmpty = allowEmpty,
    };
    if (const std::optional<Hotkey> chosen = hotkeydialog::ask(instance_, window_, request)) {
        hotkeys_[slot] = *chosen;
        settings::setHotkey(slot, *chosen);
        log::info(L"hotkey {}: now {}", slot + 1, chosen->empty() ? std::wstring(L"none") : chosen->name());
    }
    std::vector<std::wstring> failed;
    if (enabled_) {
        failed = registerHotkeys();
    }
    updateStatus();
    if (!failed.empty()) {
        announceConflict(failed);
    }
}

void App::showTrayMenu() {
    if (menuOpen_) {
        return;
    }
    if (dialogs::focusOpen()) {
        return;  // A dialog is up: a tray click just brings it back.
    }
    const HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING | checkMark(enabled_), kMenuEnabled, L"Enabled");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuHotkey, (L"Hotkey: " + hotkeyLabel(0) + L"...").c_str());
    AppendMenuW(menu, MF_STRING, kMenuSecondHotkey, (L"Second hotkey: " + hotkeyLabel(1) + L"...").c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | checkMark(settings::switchLayoutAfterConversion()),
                kMenuSwitchLayout, L"Switch keyboard layout after conversion");
    AppendMenuW(menu, MF_STRING | checkMark(settings::runAtStartup()),
                kMenuRunAtStartup, L"Run at Windows startup");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuLog, L"Log...");
    AppendMenuW(menu, MF_STRING, kMenuAbout, L"About kurva-switcher...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"Exit");

    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);  // Otherwise the menu would not close when clicking elsewhere.
    menuOpen_ = true;
    const UINT chosen = static_cast<UINT>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        cursor.x, cursor.y, window_, nullptr));
    menuOpen_ = false;
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);

    switch (chosen) {
    case kMenuEnabled:
        setEnabled(!enabled_);
        break;
    case kMenuHotkey:
        changeHotkey(0);
        break;
    case kMenuSecondHotkey:
        changeHotkey(1);
        break;
    case kMenuSwitchLayout: {
        const bool enabled = !settings::switchLayoutAfterConversion();
        settings::setSwitchLayoutAfterConversion(enabled);
        switcher_->setSwitchKeyboardLayout(enabled);
        break;
    }
    case kMenuRunAtStartup:
        toggleRunAtStartup();
        break;
    case kMenuLog:
        logdialog::show(instance_, window_);
        break;
    case kMenuAbout:
        aboutdialog::show(instance_, window_);
        break;
    case kMenuExit:
        PostQuitMessage(0);
        break;
    default:
        break;
    }
}

void App::toggleRunAtStartup() {
    if (settings::runAtStartup()) {
        if (!settings::setRunAtStartup(false)) {
            MessageBoxW(window_, L"Could not remove the startup entry from the registry.", kAppTitle, MB_ICONWARNING);
        }
        return;
    }

    const std::wstring path = settings::executablePath();
    if (settings::isTemporaryLocation(path)) {
        MessageBoxW(window_,
                    (L"kurva-switcher is running from a temporary folder:\n" + path +
                     L"\n\nWindows would not find it there after a restart. Move the executable to a "
                     L"permanent folder, start it from there and enable autostart again.")
                        .c_str(),
                    kAppTitle, MB_ICONWARNING);
        return;
    }
    if (!settings::setRunAtStartup(true)) {
        MessageBoxW(window_, L"Could not write the startup entry to the registry.", kAppTitle, MB_ICONWARNING);
    }
}

LRESULT CALLBACK App::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* self = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) {
            self->window_ = hwnd;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return self->handleMessage(message, wParam, lParam);
}

LRESULT App::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_HOTKEY:
        if (!menuOpen_ && enabled_ && switcher_) {
            switcher_->run();
        }
        return 0;

    case WM_TIMER:
        if (wParam == kRetryTimerId) {
            retryHotkeys();
        }
        return 0;

    case WM_TRAYICON:
        switch (LOWORD(lParam)) {
        case WM_CONTEXTMENU:
        case NIN_SELECT:
        case NIN_KEYSELECT:
            showTrayMenu();
            break;
        default:
            break;
        }
        return 0;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    case WM_ENDSESSION:
        if (wParam) {
            PostQuitMessage(0);  // Log-off or shutdown: leave cleanly.
        }
        return 0;

    case WM_SETTINGCHANGE:
        darkmode::handleSettingChange(lParam);  // Light/dark app mode switched while running.
        return DefWindowProcW(window_, message, wParam, lParam);

    case WM_DESTROY:
        return 0;

    default:
        if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_ != 0) {
            tray_->add();  // Explorer restarted and lost our icon.
            return 0;
        }
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

struct ComScope {
    HRESULT result;
    ComScope() : result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope() {
        if (SUCCEEDED(result)) {
            CoUninitialize();
        }
    }
};

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    const HANDLE singleInstance = CreateMutexW(nullptr, TRUE, kMutexName);
    if (singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"kurva-switcher is already running. Look for its icon in the system tray.",
                    kAppTitle, MB_ICONINFORMATION);
        CloseHandle(singleInstance);
        return 0;
    }

    int exitCode = 1;
    {
        const ComScope com;
        App app(instance);
        if (app.initialize()) {
            exitCode = app.run();
        }
    }

    if (singleInstance) {
        CloseHandle(singleInstance);
    }
    return exitCode;
}
