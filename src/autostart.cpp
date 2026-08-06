// SBShot
// Author: li xiaojun
// Module: autostart

std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::wstring();
    }
    return path;
}

std::wstring QuoteCommandPath(const std::wstring& path) {
    return L"\"" + path + L"\"";
}

bool SamePathIgnoreCase(const std::wstring& first, const std::wstring& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (size_t index = 0; index < first.size(); ++index) {
        if (std::towlower(first[index]) != std::towlower(second[index])) {
            return false;
        }
    }
    return true;
}

std::wstring ExtractExecutablePathFromCommand(const std::wstring& command) {
    if (command.empty()) {
        return std::wstring();
    }
    if (command[0] == L'"') {
        const size_t endQuote = command.find(L'"', 1);
        if (endQuote != std::wstring::npos) {
            return command.substr(1, endQuote - 1);
        }
    }
    const size_t firstSpace = command.find(L' ');
    return firstSpace == std::wstring::npos ? command : command.substr(0, firstSpace);
}

LONG OpenAutoStartKey(REGSAM access, bool create, HKEY* key) {
    if (key == NULL) {
        return ERROR_INVALID_PARAMETER;
    }
    *key = NULL;

    const REGSAM viewFlags[] = {KEY_WOW64_64KEY, 0};
    LONG lastResult = ERROR_SUCCESS;
    for (REGSAM viewFlag : viewFlags) {
        const REGSAM desiredAccess = access | viewFlag;
        lastResult = create
                         ? RegCreateKeyExW(
                               HKEY_CURRENT_USER,
                               kAutoStartRunKey,
                               0,
                               NULL,
                               0,
                               desiredAccess,
                               NULL,
                               key,
                               NULL)
                         : RegOpenKeyExW(HKEY_CURRENT_USER, kAutoStartRunKey, 0, desiredAccess, key);
        if (lastResult == ERROR_SUCCESS) {
            return ERROR_SUCCESS;
        }
    }
    return lastResult;
}

bool QueryAutoStartCommand(std::wstring* command) {
    HKEY key = NULL;
    if (OpenAutoStartKey(KEY_QUERY_VALUE, false, &key) != ERROR_SUCCESS) {
        return false;
    }

    wchar_t value[MAX_PATH * 2] = {};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    const LONG result = RegQueryValueExW(
        key,
        kAutoStartValueName,
        NULL,
        &type,
        reinterpret_cast<LPBYTE>(value),
        &bytes);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return false;
    }
    if (command != NULL) {
        *command = value;
    }
    return true;
}

bool IsAutoStartEnabled() {
    std::wstring command;
    if (!QueryAutoStartCommand(&command)) {
        return false;
    }
    return SamePathIgnoreCase(ExtractExecutablePathFromCommand(command), GetExecutablePath());
}

bool SetAutoStartEnabled(bool enabled) {
    HKEY key = NULL;
    if (OpenAutoStartKey(KEY_SET_VALUE, true, &key) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = QuoteCommandPath(GetExecutablePath());
        result = RegSetValueExW(
            key,
            kAutoStartValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1U) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kAutoStartValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void EnsureFirstRunAutoStartEnabled() {
    const std::wstring path = GetSettingsPath(true);
    if (path.empty()) {
        return;
    }
    const int userConfigured = GetPrivateProfileIntW(L"General", L"AutoStartUserConfigured", 0, path.c_str());
    if (userConfigured == 0 && !IsAutoStartEnabled() && SetAutoStartEnabled(true)) {
        WritePrivateProfileStringW(L"General", L"AutoStartInitialized", L"1", path.c_str());
    }
}

void RememberAutoStartUserConfigured() {
    const std::wstring path = GetSettingsPath(true);
    if (!path.empty()) {
        WritePrivateProfileStringW(L"General", L"AutoStartInitialized", L"1", path.c_str());
        WritePrivateProfileStringW(L"General", L"AutoStartUserConfigured", L"1", path.c_str());
    }
}
