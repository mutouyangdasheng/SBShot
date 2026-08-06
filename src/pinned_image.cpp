// SBShot
// Author: li xiaojun
// Module: pinned_image

struct PinnedImageState {
    HBITMAP bitmap = NULL;
    int width = 0;
    int height = 0;
    bool dragging = false;
    bool focused = false;
    POINT dragCursorStart = {};
    POINT dragWindowStart = {};
};


PinnedImageState* GetPinnedImageState(HWND window) {
    return reinterpret_cast<PinnedImageState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}


bool CopyPinnedImageToClipboard(HWND window, const PinnedImageState& state) {
    if (state.bitmap == NULL || state.width <= 0 || state.height <= 0) {
        return false;
    }

    HDC screenDc = GetDC(NULL);
    if (screenDc == NULL) {
        return false;
    }

    DibSurface output;
    const bool created = output.Create(screenDc, state.width, state.height);
    HDC sourceDc = CreateCompatibleDC(screenDc);
    ReleaseDC(NULL, screenDc);
    if (!created || sourceDc == NULL) {
        if (sourceDc != NULL) {
            DeleteDC(sourceDc);
        }
        return false;
    }

    HGDIOBJ oldBitmap = SelectObject(sourceDc, state.bitmap);
    const bool copied =
        oldBitmap != NULL &&
        oldBitmap != HGDI_ERROR &&
        BitBlt(output.dc(), 0, 0, state.width, state.height, sourceDc, 0, 0, SRCCOPY) != FALSE;
    if (oldBitmap != NULL && oldBitmap != HGDI_ERROR) {
        SelectObject(sourceDc, oldBitmap);
    }
    DeleteDC(sourceDc);

    return copied && CopySurfaceToClipboard(window, output, state.width, state.height);
}


bool CreatePinnedImageWindow(HBITMAP bitmap, int width, int height, POINT screenPosition) {
    if (bitmap == NULL || width <= 0 || height <= 0) {
        return false;
    }

    PinnedImageState* state = new PinnedImageState();
    state->bitmap = bitmap;
    state->width = width;
    state->height = height;

    HWND window = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kPinnedImageClassName,
        L"SBShot 贴图",
        WS_POPUP,
        screenPosition.x,
        screenPosition.y,
        width,
        height,
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        state);
    if (window == NULL) {
        delete state;
        return false;
    }

    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    SetFocus(window);
    return true;
}

bool PinSelection(HWND window, CaptureState& state) {
    CommitTextEditor(window, state);

    DibSurface output;
    int width = 0;
    int height = 0;
    if (!RenderSelectionToSurface(state, output, width, height) ||
        !CopySurfaceToClipboard(window, output, width, height)) {
        MessageBoxW(window, L"无法将截图复制到剪贴板。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    HBITMAP bitmap = output.DetachBitmap();
    POINT screenPosition = {
        state.virtualX + state.selection.left,
        state.virtualY + state.selection.top
    };
    if (!CreatePinnedImageWindow(bitmap, width, height, screenPosition)) {
        DeleteObject(bitmap);
        MessageBoxW(window, L"无法创建贴图窗口。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    DestroyWindow(window);
    return true;
}

LRESULT CALLBACK PinnedImageWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    PinnedImageState* state = GetPinnedImageState(window);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state != NULL && state->bitmap != NULL) {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            HDC memoryDc = CreateCompatibleDC(dc);
            HGDIOBJ oldBitmap = SelectObject(memoryDc, state->bitmap);
            BitBlt(dc, 0, 0, state->width, state->height, memoryDc, 0, 0, SRCCOPY);
            SelectObject(memoryDc, oldBitmap);
            DeleteDC(memoryDc);
            RECT border = {0, 0, state->width, state->height};
            HBRUSH borderBrush = CreateSolidBrush(state->focused ? RGB(37, 99, 235) : RGB(17, 24, 39));
            FrameRect(dc, &border, borderBrush != NULL ? borderBrush : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            if (borderBrush != NULL) {
                DeleteObject(borderBrush);
            }
            EndPaint(window, &paint);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state != NULL) {
            SetForegroundWindow(window);
            SetFocus(window);
            state->dragging = true;
            GetCursorPos(&state->dragCursorStart);
            RECT windowRect = {};
            GetWindowRect(window, &windowRect);
            state->dragWindowStart = {windowRect.left, windowRect.top};
            SetCapture(window);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (state != NULL && state->dragging && (wParam & MK_LBUTTON) != 0) {
            POINT cursor = {};
            GetCursorPos(&cursor);
            MoveWindow(
                window,
                state->dragWindowStart.x + cursor.x - state->dragCursorStart.x,
                state->dragWindowStart.y + cursor.y - state->dragCursorStart.y,
                state->width,
                state->height,
                TRUE);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (state != NULL && state->dragging) {
            state->dragging = false;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wParam == 'C') {
            if (state != NULL && !CopyPinnedImageToClipboard(window, *state)) {
                MessageBoxW(window, L"无法复制贴图到剪贴板。", L"SBShot", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_SETFOCUS:
        if (state != NULL) {
            state->focused = true;
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_KILLFOCUS:
        if (state != NULL) {
            state->focused = false;
            InvalidateRect(window, NULL, FALSE);
        }
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
            return TRUE;
        }
        break;
    case WM_NCDESTROY:
        if (state != NULL) {
            if (state->bitmap != NULL) {
                DeleteObject(state->bitmap);
                state->bitmap = NULL;
            }
            delete state;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}
