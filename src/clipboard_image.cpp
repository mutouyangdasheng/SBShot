// SBShot
// Author: li xiaojun
// Module: clipboard_image

bool OpenClipboardWithRetry(HWND owner) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (OpenClipboard(owner)) {
            return true;
        }
        Sleep(15);
    }
    return false;
}

// Renders the selected region plus annotations into a reusable 32-bit DIB surface.
bool RenderSelectionToSurface(CaptureState& state, DibSurface& output, int& width, int& height) {
    width = RectWidth(state.selection);
    height = RectHeight(state.selection);
    if (width <= 0 || height <= 0) {
        return false;
    }

    HDC screenDc = GetDC(NULL);
    if (screenDc == NULL) {
        return false;
    }

    const bool created = output.Create(screenDc, width, height);
    ReleaseDC(NULL, screenDc);
    if (!created) {
        return false;
    }

    if (!BitBlt(
            output.dc(), 0, 0, width, height,
            state.desktop.dc(), state.selection.left, state.selection.top, SRCCOPY)) {
        return false;
    }

    for (std::size_t index = 0; index < state.annotations.size(); ++index) {
        DrawAnnotation(output.dc(), state.annotations[index], -state.selection.left, -state.selection.top);
    }
    return true;
}

// Copies a rendered top-down DIB surface to the clipboard as CF_DIB.
bool CopySurfaceToClipboard(HWND window, const DibSurface& output, int width, int height) {
    const std::size_t stride = static_cast<std::size_t>(width) * 4U;
    const std::size_t pixelBytes = stride * static_cast<std::size_t>(height);
    if (pixelBytes / stride != static_cast<std::size_t>(height)) {
        return false;
    }

    const std::size_t totalBytes = sizeof(BITMAPINFOHEADER) + pixelBytes;
    HGLOBAL clipboardMemory = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
    if (clipboardMemory == NULL) {
        return false;
    }

    unsigned char* destination = static_cast<unsigned char*>(GlobalLock(clipboardMemory));
    if (destination == NULL) {
        GlobalFree(clipboardMemory);
        return false;
    }

    BITMAPINFOHEADER header = {};
    header.biSize = sizeof(BITMAPINFOHEADER);
    header.biWidth = width;
    header.biHeight = height;
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    header.biSizeImage = static_cast<DWORD>(pixelBytes);
    CopyMemory(destination, &header, sizeof(header));

    const unsigned char* source = static_cast<const unsigned char*>(output.bits());
    unsigned char* pixels = destination + sizeof(BITMAPINFOHEADER);
    for (int row = 0; row < height; ++row) {
        CopyMemory(
            pixels + static_cast<std::size_t>(row) * stride,
            source + static_cast<std::size_t>(height - row - 1) * stride,
            stride);
    }
    GlobalUnlock(clipboardMemory);

    if (!OpenClipboardWithRetry(window)) {
        GlobalFree(clipboardMemory);
        return false;
    }

    bool success = false;
    if (EmptyClipboard()) {
        success = SetClipboardData(CF_DIB, clipboardMemory) != NULL;
    }
    CloseClipboard();

    if (!success) {
        GlobalFree(clipboardMemory);
    }
    return success;
}

// Copies the selected region plus annotations to the clipboard as a 32-bit DIB.
bool CopySelectionToClipboard(HWND window, CaptureState& state) {
    DibSurface output;
    int width = 0;
    int height = 0;
    return RenderSelectionToSurface(state, output, width, height) &&
           CopySurfaceToClipboard(window, output, width, height);
}


bool SaveSelectionToFile(HWND window, CaptureState& state) {
    CommitTextEditor(window, state);

    DibSurface output;
    int width = 0;
    int height = 0;
    if (!RenderSelectionToSurface(state, output, width, height)) {
        MessageBoxW(window, L"无法生成截图文件。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring path;
    std::wstring mimeType;
    if (!PromptForImagePath(window, path, mimeType)) {
        return false;
    }

    if (!SaveSurfaceToImageFile(output, width, height, path, mimeType)) {
        MessageBoxW(window, L"无法保存截图文件。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    DestroyWindow(window);
    return true;
}
