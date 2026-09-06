#include "TrayIcon.h"

#include <strsafe.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "Log.h"
#include "resource.h"

namespace kurva {

namespace {

constexpr UINT kIconId = 1;
constexpr wchar_t kTitle[] = L"kurva-switcher";

// The system tray shows the icon at 16x16 (100% scaling), 20x20 (125%) and so on. The icon is
// loaded at the large size (SM_CXICON, 32x32 at 100%), which kurva.ico has as a frame of its
// own, and the shell scales it down with its own filter, as LoadIcon did in the old builds.
// (Those builds had no frame below 64x64, so asking for a small size meant a jagged shrink.
// tools/icon_frames.py now produces every size, so a small frame could be asked for directly.)
HICON loadIcon(HINSTANCE instance, bool& owned) {
    const HICON icon = static_cast<HICON>(
        LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    owned = icon != nullptr;
    return icon ? icon : LoadIconW(nullptr, IDI_APPLICATION);  // Shared: not to be destroyed.
}

// A copy of the icon with every color inverted; the alpha channel, and so the icon's outline,
// stays as it is. nullptr when the icon's bitmaps cannot be read.
HICON invertColors(HICON source) {
    ICONINFO info{};
    if (!GetIconInfo(source, &info)) {
        return nullptr;
    }
    // GetIconInfo hands out copies of the bitmaps; they are ours to delete.
    HICON result = nullptr;
    BITMAP bitmap{};
    if (info.hbmColor && GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) != 0 && bitmap.bmWidth > 0 &&
        bitmap.bmHeight > 0) {
        BITMAPINFO header{};
        header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        header.bmiHeader.biWidth = bitmap.bmWidth;
        header.bmiHeader.biHeight = -bitmap.bmHeight;  // Top-down rows.
        header.bmiHeader.biPlanes = 1;
        header.bmiHeader.biBitCount = 32;
        header.bmiHeader.biCompression = BI_RGB;
        if (const HDC screen = GetDC(nullptr)) {
            std::vector<std::uint32_t> pixels(static_cast<size_t>(bitmap.bmWidth) * static_cast<size_t>(bitmap.bmHeight));
            if (GetDIBits(screen, info.hbmColor, 0, static_cast<UINT>(bitmap.bmHeight), pixels.data(), &header,
                          DIB_RGB_COLORS) == bitmap.bmHeight) {
                for (std::uint32_t& pixel : pixels) {
                    pixel ^= 0x00FFFFFFu;  // Blue, green and red flipped; alpha kept.
                }
                void* bits = nullptr;
                const HBITMAP color = CreateDIBSection(screen, &header, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (color && bits) {
                    std::memcpy(bits, pixels.data(), pixels.size() * sizeof(std::uint32_t));
                    ICONINFO inverted = info;
                    inverted.hbmColor = color;
                    result = CreateIconIndirect(&inverted);
                }
                if (color) {
                    DeleteObject(color);
                }
            }
            ReleaseDC(nullptr, screen);
        }
    }
    if (info.hbmColor) {
        DeleteObject(info.hbmColor);
    }
    if (info.hbmMask) {
        DeleteObject(info.hbmMask);
    }
    return result;
}

}  // namespace

TrayIcon::TrayIcon(HINSTANCE instance, HWND window, UINT callbackMessage)
    : instance_(instance), window_(window), callbackMessage_(callbackMessage) {
    icon_ = loadIcon(instance_, ownsIcon_);
}

TrayIcon::~TrayIcon() {
    remove();
    if (disabledIcon_) {
        DestroyIcon(disabledIcon_);
    }
    if (ownsIcon_ && icon_) {
        DestroyIcon(icon_);
    }
}

NOTIFYICONDATAW TrayIcon::base() const {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window_;
    data.uID = kIconId;
    return data;
}

HICON TrayIcon::currentIcon() const {
    return (disabled_ && disabledIcon_) ? disabledIcon_ : icon_;
}

void TrayIcon::fillTooltip(NOTIFYICONDATAW& data) const {
    std::wstring tip = kTitle;
    if (!status_.empty()) {
        tip.append(L"\n").append(status_);
    }
    StringCchCopyW(data.szTip, ARRAYSIZE(data.szTip), tip.c_str());
}

bool TrayIcon::add() {
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = callbackMessage_;
    data.hIcon = currentIcon();
    fillTooltip(data);
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data);
    }
    added_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (!added_) {
        log::info(L"tray: the icon could not be added (error {})", GetLastError());
        return false;
    }
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

void TrayIcon::remove() {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW data = base();
    Shell_NotifyIconW(NIM_DELETE, &data);
    added_ = false;
}

void TrayIcon::setStatus(std::wstring_view status) {
    status_ = status;
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_TIP | NIF_SHOWTIP;
    fillTooltip(data);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::setDisabled(bool disabled) {
    if (disabled_ == disabled) {
        return;
    }
    disabled_ = disabled;
    if (disabled && !disabledIcon_ && ownsIcon_) {
        disabledIcon_ = invertColors(icon_);
        if (!disabledIcon_) {
            log::info(L"tray: the icon cannot be inverted (error {}); it stays as it is", GetLastError());
        }
    }
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_ICON;
    data.hIcon = currentIcon();
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::notify(std::wstring_view title, std::wstring_view text, Notice notice) {
    if (!added_) {
        return;
    }
    NOTIFYICONDATAW data = base();
    data.uFlags = NIF_INFO;
    StringCchCopyNW(data.szInfoTitle, ARRAYSIZE(data.szInfoTitle), title.data(), title.size());
    StringCchCopyNW(data.szInfo, ARRAYSIZE(data.szInfo), text.data(), text.size());
    switch (notice) {
    case Notice::Welcome:
        data.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;  // The program's own icon, large.
        data.hBalloonIcon = icon_;
        break;
    case Notice::Info:
        data.dwInfoFlags = NIIF_INFO;
        break;
    case Notice::Warning:
        data.dwInfoFlags = NIIF_WARNING;
        break;
    }
    if (!Shell_NotifyIconW(NIM_MODIFY, &data)) {
        log::info(L"tray: the notification could not be shown (error {})", GetLastError());
    }
}

}  // namespace kurva
