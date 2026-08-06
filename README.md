# SBShot

SBShot is a native Win32 screenshot tool for Windows 7 SP1, Windows 10, and Windows 11.

## Features

- Global `Ctrl+Shift+A` capture hotkey.
- Configurable capture hotkey persisted in `%APPDATA%\SBShot\settings.ini`.
- Region capture across the Windows virtual desktop.
- Line, rectangle, ellipse, and text annotations.
- Copies the completed image to the clipboard as a 32-bit DIB.
- Single-instance tray application with no third-party runtime dependency.

## Usage

1. Run `SBShot.exe`; it stays in the notification area.
2. Press `Ctrl+Shift+A` or double-click the tray icon.
3. Drag to select a region.
4. Choose Line, Rect, Ellipse, or Text from the toolbar.
5. Click Done or press Enter to copy the result to the clipboard.

Press Esc to cancel. Right-click the overlay to clear the current selection and select again.

To change the shortcut, right-click the tray icon, choose `Hotkey Settings...`, press the new key combination, and click Save. If another application already owns it, SBShot keeps the previous shortcut.

## Build

On the current workspace machine:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Release
```

For MSVC, open a developer prompt and build through CMake:

```powershell
cmake -S . -B build\msvc -A x64
cmake --build build\msvc --config Release
```

The default release build statically links the compiler C++ runtime. Windows system DLLs remain dynamically linked.
