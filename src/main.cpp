#include <windows.h>

#include <objbase.h>
#include <shellapi.h>
#include <strsafe.h>

#include <array>
#include <optional>

#include "Log.h"
#include "Settings.h"
#include "TextSwitcher.h"
#include "resource.h"

namespace {

using namespace kurva;

constexpr wchar_t kWindowClass[] = L"KurvaSwitcher.App";
constexpr wchar_t kMutexName[] = L"Local\\kurva-switcher.single-instance";
constexpr wchar_t kAppTitle[] = L"kurva-switcher";

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT kTrayIconId = 1;

enum MenuItem : UINT {
    kMenuHint = 1000,
    kMenuSwitchLayout,
    kMenuRunAtStartup,
    kMenuExit,
};

UINT checkMark(bool checked) {
    return checked ? static_cast<UINT>(MF_CHECKED) : static_cast<UINT>(MF_UNCHECKED);
}

struct Hotkey {
    int id;
    UINT modifiers;
    const wchar_t* name;
};

constexpr std::array<Hotkey, 2> kHotkeys{{
    {1, 0, L"Pause"},
    {2, MOD_SHIFT, L"Shift+Pause"},
}};

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
    int registerHotkeys();
    void unregisterHotkeys();
    void addTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();

    HINSTANCE instance_;
    HWND window_ = nullptr;
    bool classRegistered_ = false;
    UINT taskbarCreatedMessage_ = 0;
    bool trayIconAdded_ = false;
    bool menuOpen_ = false;
    std::array<bool, kHotkeys.size()> hotkeyRegistered_{};
    std::optional<TextSwitcher> switcher_;
};

App::~App() {
    unregisterHotkeys();
    removeTrayIcon();
    switcher_.reset();
    if (window_) {
        DestroyWindow(window_);
    }
    if (classRegistered_) {
        UnregisterClassW(kWindowClass, instance_);
    }
}

bool App::initialize() {
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

    if (registerHotkeys() == 0) {
        MessageBoxW(nullptr,
                    L"Could not register the Pause hotkey.\n\n"
                    L"Another program (Punto Switcher, for example) is probably using it.",
                    kAppTitle, MB_ICONERROR);
        return false;
    }

    addTrayIcon();
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

int App::registerHotkeys() {
    int registered = 0;
    for (size_t index = 0; index < kHotkeys.size(); ++index) {
        const Hotkey& hotkey = kHotkeys[index];
        // MOD_NOREPEAT keeps a held-down key from firing the conversion again and again.
        bool ok = RegisterHotKey(window_, hotkey.id, hotkey.modifiers | MOD_NOREPEAT, VK_PAUSE) != FALSE;
        if (!ok) {
            ok = RegisterHotKey(window_, hotkey.id, hotkey.modifiers, VK_PAUSE) != FALSE;
        }
        hotkeyRegistered_[index] = ok;
        if (ok) {
            ++registered;
        } else {
            log::info(L"hotkey: cannot register {} (error {})", hotkey.name, GetLastError());
        }
    }
    return registered;
}

void App::unregisterHotkeys() {
    for (size_t index = 0; index < kHotkeys.size(); ++index) {
        if (hotkeyRegistered_[index]) {
            UnregisterHotKey(window_, kHotkeys[index].id);
            hotkeyRegistered_[index] = false;
        }
    }
}

void App::addTrayIcon() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = kTrayIconId;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_TRAYICON;
    data.hIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                               LR_DEFAULTCOLOR));
    if (!data.hIcon) {
        data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip),
                   L"kurva-switcher\nPause or Shift+Pause converts the selected text");

    if (trayIconAdded_) {
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    trayIconAdded_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (trayIconAdded_) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

void App::removeTrayIcon() {
    if (!trayIconAdded_) {
        return;
    }
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    trayIconAdded_ = false;
}

void App::showTrayMenu() {
    if (menuOpen_) {
        return;
    }
    const HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }
    AppendMenuW(menu, MF_STRING | MF_GRAYED, kMenuHint, L"Pause / Shift+Pause: convert selected text");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | checkMark(settings::switchLayoutAfterConversion()),
                kMenuSwitchLayout, L"Switch keyboard layout after conversion");
    AppendMenuW(menu, MF_STRING | checkMark(settings::runAtStartup()),
                kMenuRunAtStartup, L"Run at Windows startup");
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
    case kMenuSwitchLayout: {
        const bool enabled = !settings::switchLayoutAfterConversion();
        settings::setSwitchLayoutAfterConversion(enabled);
        switcher_->setSwitchKeyboardLayout(enabled);
        break;
    }
    case kMenuRunAtStartup:
        if (!settings::setRunAtStartup(!settings::runAtStartup())) {
            MessageBoxW(window_, L"Could not update the startup setting in the registry.", kAppTitle, MB_ICONWARNING);
        }
        break;
    case kMenuExit:
        PostQuitMessage(0);
        break;
    default:
        break;
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
        if (!menuOpen_ && switcher_) {
            switcher_->run();
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

    case WM_DESTROY:
        return 0;

    default:
        if (message == taskbarCreatedMessage_ && taskbarCreatedMessage_ != 0) {
            addTrayIcon();  // Explorer restarted and lost our icon.
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
        MessageBoxW(nullptr, L"kurva-switcher is already running. Look for its icon in the notification area.",
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
