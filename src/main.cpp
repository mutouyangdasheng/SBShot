#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>

#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <cmath>
#include <string>
#include <vector>

// SBShot
// Author: li xiaojun
// Purpose: a lightweight offline Win32 screenshot and annotation tool.

namespace {

const wchar_t kMainClassName[] = L"SBShot.MainWindow";
const wchar_t kCaptureClassName[] = L"SBShot.CaptureWindow";
const wchar_t kHotkeySettingsClassName[] = L"SBShot.HotkeySettingsWindow";
const wchar_t kAboutClassName[] = L"SBShot.AboutWindow";
const wchar_t kPinnedImageClassName[] = L"SBShot.PinnedImageWindow";
const wchar_t kInstanceName[] = L"Local\\SBShot.Singleton";
const wchar_t kAutoStartRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t kAutoStartValueName[] = L"SBShot";

const UINT kTrayMessage = WM_APP + 1;
const UINT kStartCaptureMessage = WM_APP + 2;
const UINT kCaptureResultMessage = WM_APP + 3;
const UINT kCommitTextMessage = WM_APP + 4;
const UINT kCancelTextMessage = WM_APP + 5;

const UINT kPrimaryHotkeyId = 1;
const UINT kSecondaryHotkeyId = 2;
const UINT kMenuCapture = 1001;
const UINT kMenuHotkeySettings = 1002;
const UINT kMenuAbout = 1003;
const UINT kMenuAutoStart = 1004;
const UINT kMenuExit = 1005;
const UINT kHotkeyControl = 2001;
const UINT kHotkeySaveButton = 2002;
const UINT kHotkeyCancelButton = 2003;
const UINT kAboutUrlControl = 3001;
const UINT kAboutCloseButton = 3002;
const int kAppIconResourceId = 101;
const int kMinSelectionSize = 3;
const int kSelectionHitSlop = 5;
const int kResizeHandleSize = 8;
const int kToolbarButtonCount = 10;
const int kColorCount = 8;
const int kThicknessCount = 4;
const int kToolbarColorActionBase = 100;
const int kToolbarThicknessActionBase = 200;
const int kToolbarLineOptionActionBase = 300;
const int kToolbarShapeOptionActionBase = 310;
const int kSubmitActionComplete = 0;
const int kSubmitActionPin = 1;

HWND g_mainWindow = NULL;
HWND g_captureWindow = NULL;
HWND g_hotkeySettingsWindow = NULL;
HWND g_aboutWindow = NULL;
NOTIFYICONDATAW g_trayIcon = {};
bool g_hotkeyRegistered = false;
UINT g_activeHotkeyId = kPrimaryHotkeyId;
HICON g_appIcon = NULL;
HFONT g_aboutLinkFont = NULL;
ULONG_PTR g_gdiplusToken = 0;
bool g_gdiplusReady = false;
int g_defaultToolValue = 0;
int g_defaultOpenGroupValue = 0;
int g_defaultColorIndex = 0;
int g_defaultThicknessIndex = 1;
int g_defaultSubmitAction = kSubmitActionComplete;

const COLORREF kPalette[kColorCount] = {
    RGB(239, 68, 68),
    RGB(245, 158, 11),
    RGB(234, 179, 8),
    RGB(34, 197, 94),
    RGB(59, 130, 246),
    RGB(168, 85, 247),
    RGB(255, 255, 255),
    RGB(17, 24, 39)
};

const int kThicknesses[kThicknessCount] = {2, 4, 6, 8};

#include "app_config.cpp"

#include "autostart.cpp"

HICON GetAppIcon() {
    if (g_appIcon == NULL) {
        g_appIcon = static_cast<HICON>(LoadImageW(
            GetModuleHandleW(NULL),
            MAKEINTRESOURCEW(kAppIconResourceId),
            IMAGE_ICON,
            0,
            0,
            LR_DEFAULTSIZE | LR_SHARED));
    }
    return g_appIcon != NULL ? g_appIcon : LoadIconW(NULL, IDI_APPLICATION);
}

class DibSurface {
public:
    DibSurface() : dc_(NULL), bitmap_(NULL), previous_(NULL), bits_(NULL), width_(0), height_(0) {}

    ~DibSurface() {
        Reset();
    }

    DibSurface(const DibSurface&) = delete;
    DibSurface& operator=(const DibSurface&) = delete;

    bool Create(HDC referenceDc, int width, int height) {
        Reset();
        if (width <= 0 || height <= 0) {
            return false;
        }

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        dc_ = CreateCompatibleDC(referenceDc);
        if (dc_ == NULL) {
            return false;
        }

        bitmap_ = CreateDIBSection(referenceDc, &info, DIB_RGB_COLORS, &bits_, NULL, 0);
        if (bitmap_ == NULL) {
            Reset();
            return false;
        }

        previous_ = SelectObject(dc_, bitmap_);
        if (previous_ == NULL || previous_ == HGDI_ERROR) {
            Reset();
            return false;
        }

        width_ = width;
        height_ = height;
        return true;
    }

    void Reset() {
        if (dc_ != NULL && previous_ != NULL && previous_ != HGDI_ERROR) {
            SelectObject(dc_, previous_);
        }
        if (bitmap_ != NULL) {
            DeleteObject(bitmap_);
        }
        if (dc_ != NULL) {
            DeleteDC(dc_);
        }
        dc_ = NULL;
        bitmap_ = NULL;
        previous_ = NULL;
        bits_ = NULL;
        width_ = 0;
        height_ = 0;
    }

    HBITMAP DetachBitmap() {
        if (dc_ != NULL && previous_ != NULL && previous_ != HGDI_ERROR) {
            SelectObject(dc_, previous_);
        }
        if (dc_ != NULL) {
            DeleteDC(dc_);
        }
        HBITMAP detached = bitmap_;
        dc_ = NULL;
        bitmap_ = NULL;
        previous_ = NULL;
        bits_ = NULL;
        width_ = 0;
        height_ = 0;
        return detached;
    }

    HDC dc() const { return dc_; }
    HBITMAP bitmap() const { return bitmap_; }
    void* bits() const { return bits_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    HDC dc_;
    HBITMAP bitmap_;
    HGDIOBJ previous_;
    void* bits_;
    int width_;
    int height_;
};

#include "annotation.cpp"

#include "toolbar.cpp"

bool PromptForImagePath(HWND owner, std::wstring& path, std::wstring& mimeType);
bool SaveSurfaceToImageFile(
    const DibSurface& surface,
    int width,
    int height,
    const std::wstring& path,
    const std::wstring& mimeType);
void CommitTextEditor(HWND window, CaptureState& state);

#include "clipboard_image.cpp"

#include "pinned_image.cpp"

#include "capture_window.cpp"

bool AddTrayIcon(HWND window) {
    ZeroMemory(&g_trayIcon, sizeof(g_trayIcon));
    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = window;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = kTrayMessage;
    g_trayIcon.hIcon = GetAppIcon();
    const std::wstring tooltip = L"SBShot - 截图 " + FormatHotkey(g_hotkeyConfig);
    lstrcpynW(g_trayIcon.szTip, tooltip.c_str(), static_cast<int>(sizeof(g_trayIcon.szTip) / sizeof(wchar_t)));
    return Shell_NotifyIconW(NIM_ADD, &g_trayIcon) != FALSE;
}

void ShowStartupNotification() {
    const std::wstring message =
        L"SBShot 已在后台运行，按 " + FormatHotkey(g_hotkeyConfig) + L" 开始截图。";
    g_trayIcon.uFlags = NIF_INFO;
    g_trayIcon.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    lstrcpynW(g_trayIcon.szInfoTitle, L"SBShot 已启动", static_cast<int>(sizeof(g_trayIcon.szInfoTitle) / sizeof(wchar_t)));
    lstrcpynW(g_trayIcon.szInfo, message.c_str(), static_cast<int>(sizeof(g_trayIcon.szInfo) / sizeof(wchar_t)));
    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
}

void UpdateTrayTooltip() {
    const std::wstring tooltip = L"SBShot - 截图 " + FormatHotkey(g_hotkeyConfig);
    g_trayIcon.uFlags = NIF_TIP;
    lstrcpynW(g_trayIcon.szTip, tooltip.c_str(), static_cast<int>(sizeof(g_trayIcon.szTip) / sizeof(wchar_t)));
    Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
}

void UpdateTrayTooltip();

void UpdateTrayTooltip();

#include "dialogs.cpp"

void ShowTrayMenu(HWND window) {
    HMENU menu = CreatePopupMenu();
    if (menu == NULL) {
        return;
    }
    const std::wstring captureLabel = L"截图\t" + FormatHotkey(g_hotkeyConfig);
    AppendMenuW(menu, MF_STRING, kMenuCapture, captureLabel.c_str());
    AppendMenuW(menu, MF_STRING, kMenuHotkeySettings, L"快捷键设置...");
    AppendMenuW(
        menu,
        MF_STRING | (IsAutoStartEnabled() ? MF_CHECKED : MF_UNCHECKED),
        kMenuAutoStart,
        L"开机自启动");
    AppendMenuW(menu, MF_STRING, kMenuAbout, L"关于...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出");

    POINT cursor = {};
    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0, window, NULL);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

LRESULT CALLBACK MainWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (AddTrayIcon(window)) {
            ShowStartupNotification();
        }
        return 0;
    case WM_HOTKEY:
        if (wParam == g_activeHotkeyId) {
            StartCapture();
            return 0;
        }
        break;
    case kStartCaptureMessage:
        StartCapture();
        return 0;
    case kCaptureResultMessage:
        return 0;
    case kTrayMessage:
        if (lParam == WM_LBUTTONDBLCLK) {
            StartCapture();
        } else if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            ShowTrayMenu(window);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kMenuCapture) {
            StartCapture();
        } else if (LOWORD(wParam) == kMenuHotkeySettings) {
            ShowHotkeySettings(window);
        } else if (LOWORD(wParam) == kMenuAutoStart) {
            const bool enabled = !IsAutoStartEnabled();
            if (SetAutoStartEnabled(enabled)) {
                RememberAutoStartUserConfigured();
            } else {
                MessageBoxW(
                    window,
                    enabled ? L"无法开启开机自启动。" : L"无法关闭开机自启动。",
                    L"SBShot",
                    MB_OK | MB_ICONWARNING);
            }
        } else if (LOWORD(wParam) == kMenuAbout) {
            ShowAboutDialog(window);
        } else if (LOWORD(wParam) == kMenuExit) {
            DestroyWindow(window);
        }
        return 0;
    case WM_DESTROY:
        if (g_captureWindow != NULL) {
            DestroyWindow(g_captureWindow);
        }
        if (g_hotkeySettingsWindow != NULL) {
            DestroyWindow(g_hotkeySettingsWindow);
        }
        if (g_aboutWindow != NULL) {
            DestroyWindow(g_aboutWindow);
        }
        if (g_hotkeyRegistered) {
            UnregisterHotKey(window, g_activeHotkeyId);
            g_hotkeyRegistered = false;
        }
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool RegisterWindowClasses(HINSTANCE instance) {
    WNDCLASSEXW mainClass = {};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.hInstance = instance;
    mainClass.lpfnWndProc = MainWindowProc;
    mainClass.lpszClassName = kMainClassName;
    mainClass.hIcon = GetAppIcon();
    mainClass.hIconSm = GetAppIcon();
    mainClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&mainClass)) {
        return false;
    }

    WNDCLASSEXW settingsClass = {};
    settingsClass.cbSize = sizeof(settingsClass);
    settingsClass.hInstance = instance;
    settingsClass.lpfnWndProc = HotkeySettingsWindowProc;
    settingsClass.lpszClassName = kHotkeySettingsClassName;
    settingsClass.hIcon = GetAppIcon();
    settingsClass.hIconSm = GetAppIcon();
    settingsClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    settingsClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassExW(&settingsClass)) {
        return false;
    }

    WNDCLASSEXW aboutClass = {};
    aboutClass.cbSize = sizeof(aboutClass);
    aboutClass.hInstance = instance;
    aboutClass.lpfnWndProc = AboutWindowProc;
    aboutClass.lpszClassName = kAboutClassName;
    aboutClass.hIcon = GetAppIcon();
    aboutClass.hIconSm = GetAppIcon();
    aboutClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    aboutClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassExW(&aboutClass)) {
        return false;
    }

    WNDCLASSEXW pinnedClass = {};
    pinnedClass.cbSize = sizeof(pinnedClass);
    pinnedClass.hInstance = instance;
    pinnedClass.lpfnWndProc = PinnedImageWindowProc;
    pinnedClass.lpszClassName = kPinnedImageClassName;
    pinnedClass.hIcon = GetAppIcon();
    pinnedClass.hIconSm = GetAppIcon();
    pinnedClass.hCursor = LoadCursorW(NULL, IDC_SIZEALL);
    pinnedClass.hbrBackground = NULL;
    if (!RegisterClassExW(&pinnedClass)) {
        return false;
    }

    WNDCLASSEXW captureClass = {};
    captureClass.cbSize = sizeof(captureClass);
    captureClass.hInstance = instance;
    captureClass.lpfnWndProc = CaptureWindowProc;
    captureClass.lpszClassName = kCaptureClassName;
    captureClass.hCursor = LoadCursorW(NULL, IDC_CROSS);
    captureClass.hbrBackground = NULL;
    captureClass.style = CS_HREDRAW | CS_VREDRAW;
    return RegisterClassExW(&captureClass) != 0;
}

bool RunSelfTests() {
    POINT first = {40, 60};
    POINT second = {10, 20};
    RECT normalized = RectFromPoints(first, second);
    if (normalized.left != 10 || normalized.top != 20 || normalized.right != 40 || normalized.bottom != 60) {
        return false;
    }
    if (RectWidth(normalized) != 30 || RectHeight(normalized) != 40) {
        return false;
    }
    if (ClampValue(12, 0, 10) != 10 || ClampValue(-1, 0, 10) != 0) {
        return false;
    }
    if (!IsValidHotkey(kDefaultHotkey) || FormatHotkey(kDefaultHotkey) != L"Ctrl+Shift+A") {
        return false;
    }
    HotkeyConfig invalidHotkey = {0, 'A'};
    if (IsValidHotkey(invalidHotkey)) {
        return false;
    }
    return true;
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    if (std::wcsstr(GetCommandLineW(), L"--self-test") != NULL) {
        return RunSelfTests() ? 0 : 1;
    }
    const bool runHotkeyUiTest = std::wcsstr(GetCommandLineW(), L"--hotkey-ui-test") != NULL;

    ApplyDpiAwareness();
    InitCommonControls();
    Gdiplus::GdiplusStartupInput gdiplusInput = {};
    g_gdiplusReady =
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusInput, NULL) == Gdiplus::Ok;
    LoadHotkeyConfig();
    LoadAnnotationStyleConfig();
    EnsureFirstRunAutoStartEnabled();

    HANDLE instanceMutex = CreateMutexW(NULL, FALSE, kInstanceName);
    if (instanceMutex == NULL) {
        if (g_gdiplusReady) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
        }
        return 2;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kMainClassName, NULL);
        if (existing != NULL) {
            PostMessageW(existing, kStartCaptureMessage, 0, 0);
        }
        CloseHandle(instanceMutex);
        if (g_gdiplusReady) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
        }
        return 0;
    }

    if (!RegisterWindowClasses(instance)) {
        CloseHandle(instanceMutex);
        if (g_gdiplusReady) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
        }
        return 3;
    }

    g_mainWindow = CreateWindowExW(
        0, kMainClassName, L"SBShot", WS_OVERLAPPED,
        0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (g_mainWindow == NULL) {
        CloseHandle(instanceMutex);
        if (g_gdiplusReady) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
        }
        return 4;
    }

    g_activeHotkeyId = kPrimaryHotkeyId;
    g_hotkeyRegistered = RegisterHotkeyAtId(g_mainWindow, g_activeHotkeyId, g_hotkeyConfig);
    if (!g_hotkeyRegistered) {
        const std::wstring message = FormatHotkey(g_hotkeyConfig) +
            L" 已被占用。你仍可通过托盘菜单开始截图或修改快捷键。";
        MessageBoxW(
            g_mainWindow,
            message.c_str(),
            L"SBShot",
            MB_OK | MB_ICONWARNING);
    }

    if (runHotkeyUiTest) {
        ShowHotkeySettings(g_mainWindow);
        bool passed = g_hotkeySettingsWindow != NULL;
        if (passed) {
            HWND hotkeyControl = GetDlgItem(g_hotkeySettingsWindow, kHotkeyControl);
            const WORD value = static_cast<WORD>(SendMessageW(hotkeyControl, HKM_GETHOTKEY, 0, 0));
            passed = LOBYTE(value) == g_hotkeyConfig.virtualKey &&
                     ModifiersFromHotkeyControlFlags(HIBYTE(value)) == g_hotkeyConfig.modifiers;
            DestroyWindow(g_hotkeySettingsWindow);
        }
        DestroyWindow(g_mainWindow);
        CloseHandle(instanceMutex);
        if (g_gdiplusReady) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
        }
        return passed ? 0 : 5;
    }

    MSG message = {};
    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CloseHandle(instanceMutex);
    if (g_gdiplusReady) {
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
    }
    return static_cast<int>(message.wParam);
}
