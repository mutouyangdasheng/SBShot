// SBShot
// Author: li xiaojun
// Module: app_config

struct HotkeyConfig {
    UINT modifiers;
    UINT virtualKey;
};

const HotkeyConfig kDefaultHotkey = {MOD_CONTROL | MOD_SHIFT, 'A'};
HotkeyConfig g_hotkeyConfig = kDefaultHotkey;

template <typename T>
T ClampValue(T value, T minimum, T maximum) {
    return std::max(minimum, std::min(value, maximum));
}

RECT RectFromPoints(POINT first, POINT second) {
    RECT result = {};
    result.left = std::min(first.x, second.x);
    result.top = std::min(first.y, second.y);
    result.right = std::max(first.x, second.x);
    result.bottom = std::max(first.y, second.y);
    return result;
}

int RectWidth(const RECT& rect) {
    return rect.right - rect.left;
}

int RectHeight(const RECT& rect) {
    return rect.bottom - rect.top;
}

bool HotkeysEqual(const HotkeyConfig& first, const HotkeyConfig& second) {
    return first.modifiers == second.modifiers && first.virtualKey == second.virtualKey;
}

bool IsValidHotkey(const HotkeyConfig& hotkey) {
    const UINT validModifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT;
    return hotkey.virtualKey > 0 && hotkey.virtualKey < 0xFF &&
           (hotkey.modifiers & validModifiers) != 0 &&
           (hotkey.modifiers & ~validModifiers) == 0;
}

std::wstring GetSettingsPath(bool createDirectory) {
    wchar_t appData[MAX_PATH] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::wstring();
    }

    std::wstring directory(appData);
    directory += L"\\SBShot";
    if (createDirectory) {
        CreateDirectoryW(directory.c_str(), NULL);
    }
    return directory + L"\\settings.ini";
}

// Loads the persisted global capture shortcut.
void LoadHotkeyConfig() {
    const std::wstring path = GetSettingsPath(false);
    if (path.empty()) {
        return;
    }

    HotkeyConfig loaded = {};
    loaded.modifiers = static_cast<UINT>(
        GetPrivateProfileIntW(L"Hotkey", L"Modifiers", kDefaultHotkey.modifiers, path.c_str()));
    loaded.virtualKey = static_cast<UINT>(
        GetPrivateProfileIntW(L"Hotkey", L"VirtualKey", kDefaultHotkey.virtualKey, path.c_str()));
    if (IsValidHotkey(loaded)) {
        g_hotkeyConfig = loaded;
    }
}

// Saves the global capture shortcut for the next launch.
bool SaveHotkeyConfig() {
    const std::wstring path = GetSettingsPath(true);
    if (path.empty()) {
        return false;
    }

    wchar_t modifiers[16] = {};
    wchar_t virtualKey[16] = {};
    wsprintfW(modifiers, L"%u", g_hotkeyConfig.modifiers);
    wsprintfW(virtualKey, L"%u", g_hotkeyConfig.virtualKey);
    return WritePrivateProfileStringW(L"Hotkey", L"Modifiers", modifiers, path.c_str()) != FALSE &&
           WritePrivateProfileStringW(L"Hotkey", L"VirtualKey", virtualKey, path.c_str()) != FALSE;
}

// Loads the last selected annotation tool, color, and stroke thickness.
void LoadAnnotationStyleConfig() {
    const std::wstring path = GetSettingsPath(false);
    if (path.empty()) {
        return;
    }

    g_defaultToolValue = ClampValue(
        static_cast<int>(GetPrivateProfileIntW(L"Annotation", L"Tool", g_defaultToolValue, path.c_str())),
        0,
        6);
    g_defaultOpenGroupValue = ClampValue(
        static_cast<int>(GetPrivateProfileIntW(L"Annotation", L"OpenGroup", g_defaultOpenGroupValue, path.c_str())),
        0,
        2);
    g_defaultColorIndex = ClampValue(
        static_cast<int>(GetPrivateProfileIntW(L"Annotation", L"ColorIndex", g_defaultColorIndex, path.c_str())),
        0,
        kColorCount - 1);
    g_defaultThicknessIndex = ClampValue(
        static_cast<int>(
            GetPrivateProfileIntW(L"Annotation", L"ThicknessIndex", g_defaultThicknessIndex, path.c_str())),
        0,
        kThicknessCount - 1);
    g_defaultSubmitAction = ClampValue(
        static_cast<int>(GetPrivateProfileIntW(L"General", L"LastSubmitAction", g_defaultSubmitAction, path.c_str())),
        kSubmitActionComplete,
        kSubmitActionPin);
}

// Persists the current annotation defaults after the user changes a tool option.
bool SaveAnnotationStyleConfig() {
    const std::wstring path = GetSettingsPath(true);
    if (path.empty()) {
        return false;
    }

    wchar_t tool[16] = {};
    wchar_t group[16] = {};
    wchar_t color[16] = {};
    wchar_t thickness[16] = {};
    wsprintfW(tool, L"%d", g_defaultToolValue);
    wsprintfW(group, L"%d", g_defaultOpenGroupValue);
    wsprintfW(color, L"%d", g_defaultColorIndex);
    wsprintfW(thickness, L"%d", g_defaultThicknessIndex);
    return WritePrivateProfileStringW(L"Annotation", L"Tool", tool, path.c_str()) != FALSE &&
           WritePrivateProfileStringW(L"Annotation", L"OpenGroup", group, path.c_str()) != FALSE &&
           WritePrivateProfileStringW(L"Annotation", L"ColorIndex", color, path.c_str()) != FALSE &&
           WritePrivateProfileStringW(L"Annotation", L"ThicknessIndex", thickness, path.c_str()) != FALSE;
}

bool SaveSubmitActionConfig() {
    const std::wstring path = GetSettingsPath(true);
    if (path.empty()) {
        return false;
    }

    wchar_t action[16] = {};
    wsprintfW(action, L"%d", g_defaultSubmitAction);
    return WritePrivateProfileStringW(L"General", L"LastSubmitAction", action, path.c_str()) != FALSE;
}

void RememberSubmitAction(int action) {
    g_defaultSubmitAction = ClampValue(action, kSubmitActionComplete, kSubmitActionPin);
    SaveSubmitActionConfig();
}


std::wstring GetVirtualKeyName(UINT virtualKey) {
    if (virtualKey >= 'A' && virtualKey <= 'Z') {
        return std::wstring(1, static_cast<wchar_t>(virtualKey));
    }
    if (virtualKey >= '0' && virtualKey <= '9') {
        return std::wstring(1, static_cast<wchar_t>(virtualKey));
    }

    UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    LONG keyData = static_cast<LONG>(scanCode << 16);
    switch (virtualKey) {
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_UP:
    case VK_DOWN:
    case VK_DIVIDE:
    case VK_NUMLOCK:
        keyData |= 1L << 24;
        break;
    default:
        break;
    }

    wchar_t name[64] = {};
    if (GetKeyNameTextW(keyData, name, 64) > 0) {
        return name;
    }

    wchar_t fallback[16] = {};
    wsprintfW(fallback, L"VK_%02X", virtualKey);
    return fallback;
}

std::wstring FormatHotkey(const HotkeyConfig& hotkey) {
    std::wstring result;
    if ((hotkey.modifiers & MOD_CONTROL) != 0) {
        result += L"Ctrl+";
    }
    if ((hotkey.modifiers & MOD_SHIFT) != 0) {
        result += L"Shift+";
    }
    if ((hotkey.modifiers & MOD_ALT) != 0) {
        result += L"Alt+";
    }
    if ((hotkey.modifiers & MOD_WIN) != 0) {
        result += L"Win+";
    }
    result += GetVirtualKeyName(hotkey.virtualKey);
    return result;
}
