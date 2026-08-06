# SBShot Manual Test Checklist

Run this checklist on Windows 7 SP1, Windows 10, and Windows 11 before a release.

- Start the application without administrator rights and confirm that one tray icon appears.
- Press `Ctrl+Shift+A` and confirm that the capture overlay appears.
- Change the shortcut from `Hotkey Settings...`, restart SBShot, and confirm that it is restored.
- Try to save a shortcut already used by another application and confirm that the previous shortcut still works.
- Start a second instance and confirm that the existing instance starts a capture.
- Drag a selection from each direction, including right-to-left and bottom-to-top.
- Repeat selection on a monitor positioned left of or above the primary monitor.
- Draw a line, rectangle, and ellipse inside the selected region.
- Add text, press Enter to commit it, and confirm its position.
- Click Done and paste into Paint; confirm pixels and annotations match the preview.
- Press Enter after selecting and confirm that the image is copied.
- Press Esc and confirm that the overlay closes without changing the clipboard.
- Right-click after selecting and confirm that the selection resets.
- Exit from the tray menu and confirm that the global hotkey is released.
- Repeat 100 captures and confirm that handles and private memory do not grow continuously.
