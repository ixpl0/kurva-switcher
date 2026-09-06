#pragma once

#include <windows.h>

#include <shellapi.h>

#include <string>
#include <string_view>

namespace kurva {

// The frog in the notification area: its icon, its tooltip and the notifications it pops up
// ("balloons"; toasts on Windows 10 and later).
class TrayIcon {
public:
    enum class Notice { Welcome, Info, Warning };

    TrayIcon(HINSTANCE instance, HWND window, UINT callbackMessage);
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Puts the icon in the notification area. Call again when Explorer restarts (TaskbarCreated):
    // it forgets the icon then.
    bool add();
    void remove();

    // The line under the program name in the tooltip.
    void setStatus(std::wstring_view status);

    // Shows the icon in inverted colors while the hotkeys are switched off.
    void setDisabled(bool disabled);

    // Pops a notification up. A title longer than 63 characters or a text longer than 255 is cut.
    void notify(std::wstring_view title, std::wstring_view text, Notice notice);

private:
    [[nodiscard]] NOTIFYICONDATAW base() const;
    [[nodiscard]] HICON currentIcon() const;
    void fillTooltip(NOTIFYICONDATAW& data) const;

    HINSTANCE instance_;
    HWND window_;
    UINT callbackMessage_;
    HICON icon_ = nullptr;
    bool ownsIcon_ = false;
    HICON disabledIcon_ = nullptr;
    bool added_ = false;
    bool disabled_ = false;
    std::wstring status_;
};

}  // namespace kurva
