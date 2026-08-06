// SBShot
// Author: li xiaojun
// Module: dialogs

std::wstring ToLowerCopy(const std::wstring& value) {
    std::wstring result = value;
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<wchar_t>(std::towlower(result[index]));
    }
    return result;
}

std::wstring FileExtensionOf(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    const std::size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return std::wstring();
    }
    return ToLowerCopy(path.substr(dot));
}

bool GetImageEncoderClsid(const wchar_t* mimeType, CLSID& clsid) {
    UINT encoderCount = 0;
    UINT encoderBytes = 0;
    if (Gdiplus::GetImageEncodersSize(&encoderCount, &encoderBytes) != Gdiplus::Ok ||
        encoderCount == 0 ||
        encoderBytes == 0) {
        return false;
    }

    std::vector<unsigned char> buffer(encoderBytes);
    Gdiplus::ImageCodecInfo* encoders =
        reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(encoderCount, encoderBytes, encoders) != Gdiplus::Ok) {
        return false;
    }

    for (UINT index = 0; index < encoderCount; ++index) {
        if (lstrcmpiW(encoders[index].MimeType, mimeType) == 0) {
            clsid = encoders[index].Clsid;
            return true;
        }
    }
    return false;
}

bool SaveSurfaceToImageFile(
    const DibSurface& surface,
    int width,
    int height,
    const std::wstring& path,
    const std::wstring& mimeType) {
    if (!g_gdiplusReady ||
        surface.bits() == NULL ||
        width <= 0 ||
        height <= 0 ||
        path.empty() ||
        mimeType.empty()) {
        return false;
    }

    CLSID encoderClsid = {};
    if (!GetImageEncoderClsid(mimeType.c_str(), encoderClsid)) {
        return false;
    }

    Gdiplus::Bitmap bitmap(
        width,
        height,
        width * 4,
        PixelFormat32bppRGB,
        static_cast<BYTE*>(surface.bits()));

    if (lstrcmpiW(mimeType.c_str(), L"image/jpeg") == 0) {
        ULONG quality = 92;
        Gdiplus::EncoderParameters parameters = {};
        parameters.Count = 1;
        parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
        parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        parameters.Parameter[0].NumberOfValues = 1;
        parameters.Parameter[0].Value = &quality;
        return bitmap.Save(path.c_str(), &encoderClsid, &parameters) == Gdiplus::Ok;
    }

    return bitmap.Save(path.c_str(), &encoderClsid, NULL) == Gdiplus::Ok;
}

std::wstring BuildDefaultImageFileName() {
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t fileName[32] = {};
    wsprintfW(
        fileName,
        L"%04u%02u%02u%02u%02u%02u%03u.jpg",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds);
    return fileName;
}

bool PromptForImagePath(HWND owner, std::wstring& path, std::wstring& mimeType) {
    wchar_t filePath[MAX_PATH] = {};
    lstrcpynW(filePath, BuildDefaultImageFileName().c_str(), MAX_PATH);

    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter =
        L"JPEG 图片 (*.jpg;*.jpeg)\0*.jpg;*.jpeg\0PNG 图片 (*.png)\0*.png\0所有文件 (*.*)\0*.*\0";
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrDefExt = L"jpg";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetSaveFileNameW(&dialog)) {
        return false;
    }

    path = filePath;
    std::wstring extension = FileExtensionOf(path);
    if (extension.empty()) {
        const bool pngSelected = dialog.nFilterIndex == 2;
        path += pngSelected ? L".png" : L".jpg";
        extension = pngSelected ? L".png" : L".jpg";
    }

    if (extension == L".png") {
        mimeType = L"image/png";
    } else if (extension == L".jpg" || extension == L".jpeg") {
        mimeType = L"image/jpeg";
    } else {
        mimeType = dialog.nFilterIndex == 2 ? L"image/png" : L"image/jpeg";
    }
    return !path.empty() && !mimeType.empty();
}


bool RegisterHotkeyAtId(HWND window, UINT id, const HotkeyConfig& hotkey) {
    const UINT noRepeat = 0x4000U;
    return RegisterHotKey(window, id, hotkey.modifiers | noRepeat, hotkey.virtualKey) != FALSE;
}

bool TryApplyHotkey(HWND window, const HotkeyConfig& hotkey) {
    if (!IsValidHotkey(hotkey)) {
        MessageBoxW(
            g_hotkeySettingsWindow,
            L"快捷键必须包含 Ctrl、Shift 或 Alt，并搭配另一个按键。",
            L"SBShot",
            MB_OK | MB_ICONWARNING);
        return false;
    }

    if (g_hotkeyRegistered && HotkeysEqual(hotkey, g_hotkeyConfig)) {
        return true;
    }

    const UINT newId = g_activeHotkeyId == kPrimaryHotkeyId ? kSecondaryHotkeyId : kPrimaryHotkeyId;
    if (!RegisterHotkeyAtId(window, newId, hotkey)) {
        const std::wstring message = FormatHotkey(hotkey) +
            L" 已被占用，已保留当前 SBShot 快捷键。";
        MessageBoxW(g_hotkeySettingsWindow, message.c_str(), L"SBShot", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (g_hotkeyRegistered) {
        UnregisterHotKey(window, g_activeHotkeyId);
    }
    g_activeHotkeyId = newId;
    g_hotkeyRegistered = true;
    g_hotkeyConfig = hotkey;
    UpdateTrayTooltip();

    if (!SaveHotkeyConfig()) {
        MessageBoxW(
            g_hotkeySettingsWindow,
            L"快捷键已生效，但 SBShot 无法保存到下次启动。",
            L"SBShot",
            MB_OK | MB_ICONWARNING);
    }
    return true;
}

BYTE HotkeyControlFlagsFromModifiers(UINT modifiers) {
    BYTE flags = 0;
    if ((modifiers & MOD_CONTROL) != 0) {
        flags |= HOTKEYF_CONTROL;
    }
    if ((modifiers & MOD_SHIFT) != 0) {
        flags |= HOTKEYF_SHIFT;
    }
    if ((modifiers & MOD_ALT) != 0) {
        flags |= HOTKEYF_ALT;
    }
    return flags;
}

UINT ModifiersFromHotkeyControlFlags(BYTE flags) {
    UINT modifiers = 0;
    if ((flags & HOTKEYF_CONTROL) != 0) {
        modifiers |= MOD_CONTROL;
    }
    if ((flags & HOTKEYF_SHIFT) != 0) {
        modifiers |= MOD_SHIFT;
    }
    if ((flags & HOTKEYF_ALT) != 0) {
        modifiers |= MOD_ALT;
    }
    return modifiers;
}

LRESULT CALLBACK HotkeySettingsWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND label = CreateWindowExW(
            0, L"STATIC", L"截图快捷键",
            WS_CHILD | WS_VISIBLE,
            20, 20, 330, 20,
            window, NULL, GetModuleHandleW(NULL), NULL);
        HWND hotkeyControl = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"msctls_hotkey32", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            20, 44, 330, 28,
            window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHotkeyControl)), GetModuleHandleW(NULL), NULL);
        HWND saveButton = CreateWindowExW(
            0, L"BUTTON", L"保存",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            194, 92, 76, 28,
            window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHotkeySaveButton)), GetModuleHandleW(NULL), NULL);
        HWND cancelButton = CreateWindowExW(
            0, L"BUTTON", L"取消",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            278, 92, 72, 28,
            window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kHotkeyCancelButton)), GetModuleHandleW(NULL), NULL);

        SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(hotkeyControl, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(saveButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(cancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(
            hotkeyControl,
            HKM_SETHOTKEY,
            MAKEWORD(g_hotkeyConfig.virtualKey, HotkeyControlFlagsFromModifiers(g_hotkeyConfig.modifiers)),
            0);
        SetFocus(hotkeyControl);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kHotkeySaveButton) {
            HWND hotkeyControl = GetDlgItem(window, kHotkeyControl);
            const WORD value = static_cast<WORD>(SendMessageW(hotkeyControl, HKM_GETHOTKEY, 0, 0));
            HotkeyConfig hotkey = {};
            hotkey.virtualKey = LOBYTE(value);
            hotkey.modifiers = ModifiersFromHotkeyControlFlags(HIBYTE(value));
            if (TryApplyHotkey(g_mainWindow, hotkey)) {
                DestroyWindow(window);
            }
            return 0;
        }
        if (LOWORD(wParam) == kHotkeyCancelButton) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        if (g_hotkeySettingsWindow == window) {
            g_hotkeySettingsWindow = NULL;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowHotkeySettings(HWND owner) {
    if (g_hotkeySettingsWindow != NULL) {
        ShowWindow(g_hotkeySettingsWindow, SW_RESTORE);
        SetForegroundWindow(g_hotkeySettingsWindow);
        return;
    }

    RECT windowRect = {0, 0, 374, 152};
    AdjustWindowRectEx(&windowRect, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME);
    const int width = RectWidth(windowRect);
    const int height = RectHeight(windowRect);
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int left = workArea.left + (RectWidth(workArea) - width) / 2;
    const int top = workArea.top + (RectHeight(workArea) - height) / 2;

    g_hotkeySettingsWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
        kHotkeySettingsClassName,
        L"SBShot 快捷键设置",
        WS_CAPTION | WS_SYSMENU,
        left, top, width, height,
        owner, NULL, GetModuleHandleW(NULL), NULL);
    if (g_hotkeySettingsWindow != NULL) {
        ShowWindow(g_hotkeySettingsWindow, SW_SHOW);
        SetForegroundWindow(g_hotkeySettingsWindow);
    }
}

LRESULT CALLBACK AboutWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HWND title = CreateWindowExW(
            0, L"STATIC",
            L"苦于各大企业办公聊天软件截图功能久矣，于是只做纯粹的截图能力，拒绝所有无关附加功能，全程不联网，轻量无负担。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 20, 540, 46,
            window, NULL, GetModuleHandleW(NULL), NULL);
        HWND details = CreateWindowExW(
            0, L"STATIC",
            L"作者：li xiaojun\r\n"
            L"版本：0.1.0\r\n\r\n"
            L"设计目的：\r\n"
            L"支持快速区域截图、常用标注、颜色和粗细设置，并将结果复制到剪贴板。\r\n"
            L"SBShot 绝不是骂人，是 smartBoyShot 简写。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 78, 540, 112,
            window, NULL, GetModuleHandleW(NULL), NULL);
        HWND url = CreateWindowExW(
            0, L"STATIC", L"https://www.smartboy.fun",
            WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_LEFT,
            24, 190, 250, 20,
            window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kAboutUrlControl)), GetModuleHandleW(NULL), NULL);
        HWND copyright = CreateWindowExW(
            0, L"STATIC", L"Copyright © 2026 smartboy.fun. All rights reserved.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 218, 520, 20,
            window, NULL, GetModuleHandleW(NULL), NULL);
        HWND hotkey = CreateWindowExW(
            0, L"STATIC", L"默认快捷键：Ctrl+Shift+A",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            24, 244, 260, 20,
            window, NULL, GetModuleHandleW(NULL), NULL);
        HWND close = CreateWindowExW(
            0, L"BUTTON", L"确定",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            488, 276, 76, 28,
            window, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(kAboutCloseButton)), GetModuleHandleW(NULL), NULL);

        LOGFONTW linkFontInfo = {};
        GetObjectW(font, sizeof(linkFontInfo), &linkFontInfo);
        linkFontInfo.lfUnderline = TRUE;
        g_aboutLinkFont = CreateFontIndirectW(&linkFontInfo);

        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(details, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(url, WM_SETFONT, reinterpret_cast<WPARAM>(g_aboutLinkFont != NULL ? g_aboutLinkFont : font), TRUE);
        SendMessageW(copyright, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(hotkey, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        SendMessageW(close, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == kAboutUrlControl) {
            ShellExecuteW(NULL, L"open", L"https://www.smartboy.fun", NULL, NULL, SW_SHOWNORMAL);
            return 0;
        }
        if (LOWORD(wParam) == kAboutCloseButton) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HWND child = reinterpret_cast<HWND>(lParam);
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        if (GetDlgCtrlID(child) == kAboutUrlControl) {
            SetTextColor(dc, RGB(37, 99, 235));
            SetCursor(LoadCursorW(NULL, IDC_HAND));
        } else {
            SetTextColor(dc, RGB(17, 24, 39));
        }
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    }
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        if (g_aboutWindow == window) {
            g_aboutWindow = NULL;
        }
        if (g_aboutLinkFont != NULL) {
            DeleteObject(g_aboutLinkFont);
            g_aboutLinkFont = NULL;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

// Shows product information with a clickable project URL.
void ShowAboutDialog(HWND owner) {
    if (g_aboutWindow != NULL) {
        ShowWindow(g_aboutWindow, SW_RESTORE);
        SetForegroundWindow(g_aboutWindow);
        return;
    }

    RECT windowRect = {0, 0, 588, 330};
    AdjustWindowRectEx(&windowRect, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME);
    const int width = RectWidth(windowRect);
    const int height = RectHeight(windowRect);
    RECT workArea = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int left = workArea.left + (RectWidth(workArea) - width) / 2;
    const int top = workArea.top + (RectHeight(workArea) - height) / 2;

    g_aboutWindow = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
        kAboutClassName,
        L"关于 SBShot",
        WS_CAPTION | WS_SYSMENU,
        left, top, width, height,
        owner, NULL, GetModuleHandleW(NULL), NULL);
    if (g_aboutWindow != NULL) {
        ShowWindow(g_aboutWindow, SW_SHOW);
        SetForegroundWindow(g_aboutWindow);
    }
}
