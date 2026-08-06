// SBShot
// Author: li xiaojun
// Module: capture_window

void ApplyDpiAwareness() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32 != NULL) {
        typedef BOOL(WINAPI* SetProcessDpiAwarenessContextFunction)(HANDLE);
        SetProcessDpiAwarenessContextFunction setContext =
            reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext != NULL) {
            HANDLE perMonitorV2 = reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4));
            if (setContext(perMonitorV2)) {
                return;
            }
        }
    }
    if (user32 != NULL) {
        typedef BOOL(WINAPI* SetProcessDpiAwareFunction)();
        SetProcessDpiAwareFunction setAware =
            reinterpret_cast<SetProcessDpiAwareFunction>(GetProcAddress(user32, "SetProcessDPIAware"));
        if (setAware != NULL) {
            setAware();
        }
    }
}

// Captures the full virtual desktop into an offscreen DIB used by the overlay.
bool CaptureDesktop(CaptureState& state) {
    state.virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    state.virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    state.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    state.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (state.width <= 0 || state.height <= 0) {
        return false;
    }

    HDC screenDc = GetDC(NULL);
    if (screenDc == NULL) {
        return false;
    }

    bool created = state.desktop.Create(screenDc, state.width, state.height);
    if (created) {
        created = BitBlt(
            state.desktop.dc(), 0, 0, state.width, state.height,
            screenDc, state.virtualX, state.virtualY, SRCCOPY | CAPTUREBLT) != FALSE;
    }

    if (created) {
        created = state.dimPixel.Create(screenDc, 1, 1);
        if (created) {
            *static_cast<std::uint32_t*>(state.dimPixel.bits()) = 0x00000000U;
        }
    }

    ReleaseDC(NULL, screenDc);
    return created;
}


void DrawSelectionHandles(HDC dc, const RECT& selection) {
    RECT handles[8] = {};
    GetSelectionHandleRects(selection, handles);

    HBRUSH fill = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH frame = CreateSolidBrush(RGB(35, 133, 235));
    if (fill == NULL || frame == NULL) {
        if (fill != NULL) {
            DeleteObject(fill);
        }
        if (frame != NULL) {
            DeleteObject(frame);
        }
        return;
    }

    for (int index = 0; index < 8; ++index) {
        FillRect(dc, &handles[index], fill);
        FrameRect(dc, &handles[index], frame);
    }

    DeleteObject(frame);
    DeleteObject(fill);
}

// Paints the dimmed overlay, selected region, annotations, handles, and toolbar.
void PaintCaptureWindow(HWND window, CaptureState& state, HDC dc) {
    BitBlt(dc, 0, 0, state.width, state.height, state.desktop.dc(), 0, 0, SRCCOPY);

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 112;
    AlphaBlend(dc, 0, 0, state.width, state.height, state.dimPixel.dc(), 0, 0, 1, 1, blend);

    if (!state.hasSelection && !state.selecting) {
        return;
    }

    RECT selection = state.selecting ? RectFromPoints(state.dragStart, state.dragEnd) : state.selection;
    if (RectWidth(selection) <= 0 || RectHeight(selection) <= 0) {
        return;
    }

    BitBlt(
        dc, selection.left, selection.top, RectWidth(selection), RectHeight(selection),
        state.desktop.dc(), selection.left, selection.top, SRCCOPY);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(35, 133, 235));
    HGDIOBJ oldPen = SelectObject(dc, borderPen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, selection.left, selection.top, selection.right, selection.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(borderPen);

    if (!state.hasSelection) {
        return;
    }

    const int annotationClip = SaveDC(dc);
    IntersectClipRect(dc, selection.left, selection.top, selection.right, selection.bottom);
    for (std::size_t index = 0; index < state.annotations.size(); ++index) {
        DrawAnnotation(dc, state.annotations[index], 0, 0);
    }

    if (state.drawing) {
        Annotation preview = CreateStyledAnnotation(state, state.tool, state.dragStart, state.dragEnd, std::wstring());
        DrawAnnotation(dc, preview, 0, 0);
    }
    RestoreDC(dc, annotationClip);

    DrawSelectionHandles(dc, selection);

    wchar_t dimensions[64] = {};
    wsprintfW(dimensions, L"%d x %d", RectWidth(selection), RectHeight(selection));
    RECT sizeBackground = {selection.left, std::max(0L, selection.top - 24), selection.left + 100, selection.top};
    HBRUSH sizeBrush = CreateSolidBrush(RGB(35, 133, 235));
    FillRect(dc, &sizeBackground, sizeBrush);
    DeleteObject(sizeBrush);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HGDIOBJ sizeFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    TextOutW(dc, sizeBackground.left + 6, sizeBackground.top + 5, dimensions, static_cast<int>(std::wcslen(dimensions)));
    SelectObject(dc, sizeFont);

    DrawToolbar(dc, state);
    (void)window;
}


void DestroyTextEditor(CaptureState& state) {
    state.editDragging = false;
    state.editDragMoved = false;
    if (state.editWindow != NULL) {
        HWND edit = state.editWindow;
        state.editWindow = NULL;
        DestroyWindow(edit);
    }
    if (state.editFont != NULL) {
        DeleteObject(state.editFont);
        state.editFont = NULL;
    }
}

void CommitTextEditor(HWND window, CaptureState& state) {
    if (state.editWindow == NULL) {
        return;
    }

    const int length = GetWindowTextLengthW(state.editWindow);
    std::wstring text;
    if (length > 0 && length < 4096) {
        std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1U, L'\0');
        GetWindowTextW(state.editWindow, buffer.data(), length + 1);
        text.assign(buffer.data());
    }

    DestroyTextEditor(state);
    if (!text.empty()) {
        Annotation annotation = CreateStyledAnnotation(state, Tool::Text, state.textAnchor, state.textAnchor, text);
        AddAnnotation(state, annotation);
    }
    SetFocus(window);
    InvalidateRect(window, NULL, FALSE);
}

LRESULT CALLBACK TextEditWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    WNDPROC original = reinterpret_cast<WNDPROC>(GetPropW(window, L"SBShot.OriginalEditProc"));
    HWND parent = GetParent(window);
    CaptureState* state = GetCaptureState(parent);
    if (message == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            SendMessageW(parent, kCommitTextMessage, 0, 0);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            SendMessageW(parent, kCancelTextMessage, 0, 0);
            return 0;
        }
    }
    if (message == WM_LBUTTONDOWN && state != NULL) {
        SetFocus(window);
        SendMessageW(window, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
        state->editDragging = true;
        state->editDragMoved = false;
        state->editDragCursorStart = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ClientToScreen(window, &state->editDragCursorStart);
        RECT editRect = {};
        GetWindowRect(window, &editRect);
        POINT editOrigin = {editRect.left, editRect.top};
        ScreenToClient(parent, &editOrigin);
        state->editDragWindowStart = editOrigin;
        state->editDragAnchorStart = state->textAnchor;
        SetCapture(window);
        return 0;
    }
    if (message == WM_MOUSEMOVE && state != NULL && state->editDragging && (wParam & MK_LBUTTON) != 0) {
        POINT cursor = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ClientToScreen(window, &cursor);
        const int dx = cursor.x - state->editDragCursorStart.x;
        const int dy = cursor.y - state->editDragCursorStart.y;
        if (!state->editDragMoved && dx * dx + dy * dy < 9) {
            return 0;
        }
        state->editDragMoved = true;
        RECT editRect = {};
        GetWindowRect(window, &editRect);
        const int width = RectWidth(editRect);
        const int height = RectHeight(editRect);
        POINT position = ClampTextEditorPosition(
            *state,
            state->editDragWindowStart.x + dx,
            state->editDragWindowStart.y + dy,
            width,
            height);
        MoveWindow(window, position.x, position.y, width, height, TRUE);
        state->textAnchor.x = ClampValue(
            static_cast<int>(state->editDragAnchorStart.x + position.x - state->editDragWindowStart.x),
            static_cast<int>(state->selection.left),
            static_cast<int>(state->selection.right - 1));
        state->textAnchor.y = ClampValue(
            static_cast<int>(state->editDragAnchorStart.y + position.y - state->editDragWindowStart.y),
            static_cast<int>(state->selection.top),
            static_cast<int>(state->selection.bottom - 1));
        return 0;
    }
    if (message == WM_LBUTTONUP && state != NULL && state->editDragging) {
        const bool moved = state->editDragMoved;
        state->editDragging = false;
        state->editDragMoved = false;
        ReleaseCapture();
        SetFocus(window);
        if (!moved) {
            SendMessageW(window, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
        }
        return 0;
    }
    if (message == WM_SETCURSOR) {
        SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
        return TRUE;
    }
    if (message == WM_NCDESTROY) {
        RemovePropW(window, L"SBShot.OriginalEditProc");
    }
    return original != NULL ? CallWindowProcW(original, window, message, wParam, lParam)
                            : DefWindowProcW(window, message, wParam, lParam);
}

// Opens the inline text editor using the same font and color as final text.
void BeginTextEditor(HWND window, CaptureState& state, POINT anchor) {
    if (state.editWindow != NULL) {
        CommitTextEditor(window, state);
    }

    state.textAnchor = ClampToSelection(state, anchor);
    const int availableRight =
        std::max(1, static_cast<int>(state.selection.right) - static_cast<int>(state.textAnchor.x));
    const int editorWidth = std::min(420, availableRight);
    const int editorHeight = std::min(42, std::max(1, RectHeight(state.selection)));
    int left = state.textAnchor.x;
    int top = state.textAnchor.y;
    if (top + editorHeight > state.selection.bottom) {
        top = state.selection.bottom - editorHeight;
    }
    POINT editorPosition = ClampTextEditorPosition(state, left, top, editorWidth, editorHeight);

    state.editWindow = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        editorPosition.x, editorPosition.y, editorWidth, editorHeight,
        window, NULL, GetModuleHandleW(NULL), NULL);
    if (state.editWindow == NULL) {
        return;
    }

    state.editFont = CreateAnnotationFont();
    SendMessageW(
        state.editWindow,
        WM_SETFONT,
        reinterpret_cast<WPARAM>(state.editFont != NULL ? state.editFont : GetStockObject(DEFAULT_GUI_FONT)),
        TRUE);
    WNDPROC original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(state.editWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TextEditWindowProc)));
    SetPropW(state.editWindow, L"SBShot.OriginalEditProc", reinterpret_cast<HANDLE>(original));
    SetFocus(state.editWindow);
}

void FinishCapture(HWND window, CaptureState& state) {
    CommitTextEditor(window, state);
    if (CopySelectionToClipboard(window, state)) {
        DestroyWindow(window);
    } else {
        MessageBoxW(window, L"无法将截图复制到剪贴板。", L"SBShot", MB_OK | MB_ICONERROR);
    }
}

void ExecuteSubmitAction(HWND window, CaptureState& state, int action, bool rememberAction) {
    const int submitAction = ClampValue(action, kSubmitActionComplete, kSubmitActionPin);
    if (rememberAction) {
        RememberSubmitAction(submitAction);
    }

    if (submitAction == kSubmitActionPin) {
        PinSelection(window, state);
    } else {
        FinishCapture(window, state);
    }
}

void ResetSelection(HWND window, CaptureState& state) {
    DestroyTextEditor(state);
    state.selecting = false;
    state.drawing = false;
    state.adjustingSelection = false;
    state.hasSelection = false;
    state.selectionDragMode = SelectionDragMode::None;
    state.tool = Tool::None;
    state.openGroup = ToolGroup::None;
    state.hoverAction = -1;
    state.annotations.clear();
    state.redoAnnotations.clear();
    state.currentPoints.clear();
    InvalidateRect(window, NULL, FALSE);
}

// Applies toolbar actions, including grouped tools and style persistence.
void HandleToolbarAction(HWND window, CaptureState& state, int action) {
    if (action >= kToolbarLineOptionActionBase && action < kToolbarLineOptionActionBase + 2) {
        if (state.editWindow != NULL) {
            CommitTextEditor(window, state);
        }
        state.tool = action == kToolbarLineOptionActionBase ? Tool::Line : Tool::Arrow;
        state.openGroup = ToolGroup::Line;
        RememberAnnotationStyle(state);
        InvalidateRect(window, NULL, FALSE);
        return;
    }

    if (action >= kToolbarShapeOptionActionBase && action < kToolbarShapeOptionActionBase + 2) {
        if (state.editWindow != NULL) {
            CommitTextEditor(window, state);
        }
        state.tool = action == kToolbarShapeOptionActionBase ? Tool::RoundedRectangle : Tool::Ellipse;
        state.openGroup = ToolGroup::Shape;
        RememberAnnotationStyle(state);
        InvalidateRect(window, NULL, FALSE);
        return;
    }

    if (action >= kToolbarColorActionBase && action < kToolbarColorActionBase + kColorCount) {
        state.colorIndex = action - kToolbarColorActionBase;
        RememberAnnotationStyle(state);
        if (state.editWindow != NULL) {
            InvalidateRect(state.editWindow, NULL, TRUE);
        }
        InvalidateRect(window, NULL, FALSE);
        return;
    }

    if (action >= kToolbarThicknessActionBase && action < kToolbarThicknessActionBase + kThicknessCount) {
        state.thicknessIndex = action - kToolbarThicknessActionBase;
        RememberAnnotationStyle(state);
        InvalidateRect(window, NULL, FALSE);
        return;
    }

    if (action >= 2 && action <= 5 && state.editWindow != NULL) {
        CommitTextEditor(window, state);
    }

    switch (action) {
    case 0:
        CommitTextEditor(window, state);
        if (UndoAnnotation(state)) {
            InvalidateRect(window, NULL, FALSE);
        }
        return;
    case 1:
        CommitTextEditor(window, state);
        if (RedoAnnotation(state)) {
            InvalidateRect(window, NULL, FALSE);
        }
        return;
    case 2:
        state.openGroup = state.openGroup == ToolGroup::Line ? ToolGroup::None : ToolGroup::Line;
        if (!IsLineGroupTool(state.tool)) {
            state.tool = Tool::Line;
        }
        RememberAnnotationStyle(state);
        break;
    case 3:
        state.openGroup = state.openGroup == ToolGroup::Shape ? ToolGroup::None : ToolGroup::Shape;
        if (!IsShapeGroupTool(state.tool)) {
            state.tool = Tool::RoundedRectangle;
        }
        RememberAnnotationStyle(state);
        break;
    case 4:
        state.tool = Tool::Doodle;
        state.openGroup = ToolGroup::None;
        RememberAnnotationStyle(state);
        break;
    case 5:
        state.tool = Tool::Text;
        state.openGroup = ToolGroup::None;
        RememberAnnotationStyle(state);
        break;
    case 6:
        ExecuteSubmitAction(window, state, kSubmitActionComplete, true);
        return;
    case 7:
        DestroyWindow(window);
        return;
    case 8:
        ExecuteSubmitAction(window, state, kSubmitActionPin, true);
        return;
    case 9:
        SaveSelectionToFile(window, state);
        return;
    default:
        return;
    }
    InvalidateRect(window, NULL, FALSE);
}

// Main overlay window procedure for selection, annotation, resizing, and hotkeys.
LRESULT CALLBACK CaptureWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    CaptureState* state = GetCaptureState(window);

    switch (message) {
    case WM_NCCREATE: {
        CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (state != NULL) {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            if (state->frame.width() != state->width || state->frame.height() != state->height) {
                state->frame.Create(dc, state->width, state->height);
            }
            if (state->frame.dc() != NULL) {
                PaintCaptureWindow(window, *state, state->frame.dc());
                BitBlt(dc, 0, 0, state->width, state->height, state->frame.dc(), 0, 0, SRCCOPY);
            } else {
                PaintCaptureWindow(window, *state, dc);
            }
            EndPaint(window, &paint);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (state != NULL) {
            POINT point = ClampToClient(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            SetFocus(window);
            if (state->hasSelection) {
                const int action = ToolbarHitTest(*state, point);
                if (action >= 0) {
                    HandleToolbarAction(window, *state, action);
                    return 0;
                }

                const SelectionDragMode dragMode = HitTestSelectionDrag(*state, point);
                if (dragMode != SelectionDragMode::None) {
                    CommitTextEditor(window, *state);
                    state->adjustingSelection = true;
                    state->selectionDragMode = dragMode;
                    state->selectionDragStart = point;
                    state->selectionDragOriginal = state->selection;
                    SetCapture(window);
                    InvalidateRect(window, NULL, FALSE);
                    return 0;
                }

                if (PtInRect(&state->selection, point)) {
                    if (state->tool == Tool::Text) {
                        BeginTextEditor(window, *state, point);
                        return 0;
                    } else if (IsDrawingTool(state->tool)) {
                        state->drawing = true;
                        state->dragStart = ClampToSelection(*state, point);
                        state->dragEnd = state->dragStart;
                        state->currentPoints.clear();
                        if (state->tool == Tool::Doodle) {
                            state->currentPoints.push_back(state->dragStart);
                        }
                        SetCapture(window);
                    }
                }
            } else {
                state->selecting = true;
                state->dragStart = point;
                state->dragEnd = point;
                SetCapture(window);
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_MOUSEMOVE:
        if (state != NULL && state->adjustingSelection) {
            if (state->hoverAction != -1) {
                state->hoverAction = -1;
            }
            POINT point = ClampToClient(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            RECT adjusted = AdjustSelectionRect(*state, point);
            if (state->selectionDragMode == SelectionDragMode::Move) {
                const int dx = adjusted.left - state->selection.left;
                const int dy = adjusted.top - state->selection.top;
                OffsetAnnotations(*state, dx, dy);
            }
            state->selection = adjusted;
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (state != NULL && (state->selecting || state->drawing)) {
            if (state->hoverAction != -1) {
                state->hoverAction = -1;
            }
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            state->dragEnd = state->drawing ? ClampToSelection(*state, point) : ClampToClient(*state, point);
            if (state->drawing && state->tool == Tool::Doodle) {
                if (state->currentPoints.empty()) {
                    state->currentPoints.push_back(state->dragEnd);
                } else {
                    const POINT last = state->currentPoints.back();
                    const int dx = state->dragEnd.x - last.x;
                    const int dy = state->dragEnd.y - last.y;
                    if (dx * dx + dy * dy >= 4) {
                        state->currentPoints.push_back(state->dragEnd);
                    }
                }
            }
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (state != NULL && state->hasSelection) {
            POINT point = ClampToClient(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            const int hoverAction = ToolbarHitTest(*state, point);
            if (hoverAction != state->hoverAction) {
                state->hoverAction = hoverAction;
                state->hoverPoint = point;
                InvalidateRect(window, NULL, FALSE);
            }
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (state != NULL && state->adjustingSelection) {
            POINT point = ClampToClient(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            RECT adjusted = AdjustSelectionRect(*state, point);
            if (state->selectionDragMode == SelectionDragMode::Move) {
                const int dx = adjusted.left - state->selection.left;
                const int dy = adjusted.top - state->selection.top;
                OffsetAnnotations(*state, dx, dy);
            }
            state->selection = adjusted;
            state->adjustingSelection = false;
            state->selectionDragMode = SelectionDragMode::None;
            ReleaseCapture();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (state != NULL && state->selecting) {
            state->dragEnd = ClampToClient(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            state->selection = RectFromPoints(state->dragStart, state->dragEnd);
            state->selecting = false;
            state->hasSelection =
                RectWidth(state->selection) >= kMinSelectionSize &&
                RectHeight(state->selection) >= kMinSelectionSize;
            ReleaseCapture();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        if (state != NULL && state->drawing) {
            state->dragEnd = ClampToSelection(*state, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
            Annotation annotation =
                CreateStyledAnnotation(*state, state->tool, state->dragStart, state->dragEnd, std::wstring());
            AddAnnotation(*state, annotation);
            state->drawing = false;
            state->currentPoints.clear();
            ReleaseCapture();
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_RBUTTONDOWN:
        if (state != NULL) {
            ResetSelection(window, *state);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (state != NULL) {
            const bool controlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (controlDown && wParam == 'Z') {
                CommitTextEditor(window, *state);
                if (UndoAnnotation(*state)) {
                    InvalidateRect(window, NULL, FALSE);
                }
                return 0;
            }
            if (controlDown && wParam == 'Y') {
                CommitTextEditor(window, *state);
                if (RedoAnnotation(*state)) {
                    InvalidateRect(window, NULL, FALSE);
                }
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                DestroyWindow(window);
                return 0;
            }
            if ((wParam == VK_RETURN || wParam == VK_SPACE) && state->hasSelection) {
                ExecuteSubmitAction(window, *state, g_defaultSubmitAction, false);
                return 0;
            }
            if (wParam == 'L') {
                state->tool = Tool::Line;
                state->openGroup = ToolGroup::Line;
            } else if (wParam == 'A') {
                state->tool = Tool::Arrow;
                state->openGroup = ToolGroup::Line;
            } else if (wParam == 'R') {
                state->tool = Tool::RoundedRectangle;
                state->openGroup = ToolGroup::Shape;
            } else if (wParam == 'E') {
                state->tool = Tool::Ellipse;
                state->openGroup = ToolGroup::Shape;
            } else if (wParam == 'D') {
                state->tool = Tool::Doodle;
                state->openGroup = ToolGroup::None;
            } else if (wParam == 'T') {
                state->tool = Tool::Text;
                state->openGroup = ToolGroup::None;
            }
            RememberAnnotationStyle(*state);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case kCommitTextMessage:
        if (state != NULL) {
            CommitTextEditor(window, *state);
            return 0;
        }
        break;
    case kCancelTextMessage:
        if (state != NULL) {
            DestroyTextEditor(*state);
            SetFocus(window);
            InvalidateRect(window, NULL, FALSE);
            return 0;
        }
        break;
    case WM_CTLCOLOREDIT:
        if (state != NULL && reinterpret_cast<HWND>(lParam) == state->editWindow) {
            HDC editDc = reinterpret_cast<HDC>(wParam);
            const COLORREF textColor = CurrentColor(*state);
            const bool lightText = IsLightColor(textColor);
            SetTextColor(editDc, textColor);
            SetBkColor(editDc, lightText ? RGB(17, 24, 39) : RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(
                lightText ? GetStockObject(BLACK_BRUSH) : GetStockObject(WHITE_BRUSH));
        }
        break;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            HCURSOR cursor = NULL;
            if (state != NULL && state->hasSelection && !state->selecting && !state->drawing) {
                POINT cursorPoint = {};
                GetCursorPos(&cursorPoint);
                ScreenToClient(window, &cursorPoint);
                const SelectionDragMode dragMode = HitTestSelectionDrag(*state, cursorPoint);
                cursor = CursorForSelectionDragMode(dragMode);
                if (cursor == NULL && state->tool == Tool::Text && PtInRect(&state->selection, cursorPoint)) {
                    cursor = LoadCursorW(NULL, IDC_IBEAM);
                }
            }
            if (cursor == NULL) {
                cursor = LoadCursorW(NULL, IDC_CROSS);
            }
            SetCursor(cursor);
            return TRUE;
        }
        break;
    case WM_NCDESTROY:
        if (state != NULL) {
            DestroyTextEditor(*state);
            delete state;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        }
        if (g_captureWindow == window) {
            g_captureWindow = NULL;
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

// Starts a new capture overlay and seeds it with the last used annotation style.
bool StartCapture() {
    if (g_captureWindow != NULL) {
        SetForegroundWindow(g_captureWindow);
        return true;
    }

    CaptureState* state = new CaptureState();
    state->tool = ToolFromValue(g_defaultToolValue);
    state->openGroup = ToolGroupFromValue(g_defaultOpenGroupValue);
    state->openGroup = PreferredGroupForTool(state->tool, state->openGroup);
    state->colorIndex = ClampValue(g_defaultColorIndex, 0, kColorCount - 1);
    state->thicknessIndex = ClampValue(g_defaultThicknessIndex, 0, kThicknessCount - 1);
    if (!CaptureDesktop(*state)) {
        delete state;
        MessageBoxW(g_mainWindow, L"无法捕获桌面。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    g_captureWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kCaptureClassName,
        L"SBShot 截图",
        WS_POPUP,
        state->virtualX,
        state->virtualY,
        state->width,
        state->height,
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        state);

    if (g_captureWindow == NULL) {
        delete state;
        MessageBoxW(g_mainWindow, L"无法创建截图覆盖层。", L"SBShot", MB_OK | MB_ICONERROR);
        return false;
    }

    ShowWindow(g_captureWindow, SW_SHOW);
    UpdateWindow(g_captureWindow);
    SetForegroundWindow(g_captureWindow);
    SetFocus(g_captureWindow);
    return true;
}
