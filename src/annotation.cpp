// SBShot
// Author: li xiaojun
// Module: annotation

enum class Tool {
    None,
    Line,
    Arrow,
    RoundedRectangle,
    Ellipse,
    Doodle,
    Text
};

enum class ToolGroup {
    None,
    Line,
    Shape
};

enum class SelectionDragMode {
    None,
    Move,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight
};

struct Annotation {
    Tool tool = Tool::None;
    POINT start = {};
    POINT end = {};
    std::wstring text;
    COLORREF color = RGB(230, 57, 70);
    int thickness = 3;
    std::vector<POINT> points;
};

struct CaptureState {
    DibSurface desktop;
    DibSurface dimPixel;
    DibSurface frame;
    int virtualX = 0;
    int virtualY = 0;
    int width = 0;
    int height = 0;
    bool selecting = false;
    bool hasSelection = false;
    bool drawing = false;
    bool adjustingSelection = false;
    POINT dragStart = {};
    POINT dragEnd = {};
    POINT selectionDragStart = {};
    RECT selectionDragOriginal = {};
    RECT selection = {};
    SelectionDragMode selectionDragMode = SelectionDragMode::None;
    Tool tool = Tool::None;
    ToolGroup openGroup = ToolGroup::None;
    int colorIndex = 0;
    int thicknessIndex = 1;
    int hoverAction = -1;
    POINT hoverPoint = {};
    std::vector<Annotation> annotations;
    std::vector<Annotation> redoAnnotations;
    std::vector<POINT> currentPoints;
    HWND editWindow = NULL;
    HFONT editFont = NULL;
    POINT textAnchor = {};
    bool editDragging = false;
    bool editDragMoved = false;
    POINT editDragCursorStart = {};
    POINT editDragWindowStart = {};
    POINT editDragAnchorStart = {};
};


CaptureState* GetCaptureState(HWND window) {
    return reinterpret_cast<CaptureState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
}


POINT ClampToClient(const CaptureState& state, POINT point) {
    point.x = ClampValue(point.x, 0L, static_cast<LONG>(state.width));
    point.y = ClampValue(point.y, 0L, static_cast<LONG>(state.height));
    return point;
}

POINT ClampToSelection(const CaptureState& state, POINT point) {
    point.x = ClampValue(point.x, state.selection.left, state.selection.right - 1);
    point.y = ClampValue(point.y, state.selection.top, state.selection.bottom - 1);
    return point;
}

bool IsDrawingTool(Tool tool) {
    return tool == Tool::Line ||
           tool == Tool::Arrow ||
           tool == Tool::RoundedRectangle ||
           tool == Tool::Ellipse ||
           tool == Tool::Doodle;
}

bool IsLineGroupTool(Tool tool) {
    return tool == Tool::Line || tool == Tool::Arrow;
}

bool IsShapeGroupTool(Tool tool) {
    return tool == Tool::RoundedRectangle || tool == Tool::Ellipse;
}

Tool ToolFromValue(int value) {
    switch (value) {
    case 1:
        return Tool::Line;
    case 2:
        return Tool::Arrow;
    case 3:
        return Tool::RoundedRectangle;
    case 4:
        return Tool::Ellipse;
    case 5:
        return Tool::Doodle;
    case 6:
        return Tool::Text;
    case 0:
    default:
        return Tool::None;
    }
}

ToolGroup ToolGroupFromValue(int value) {
    switch (value) {
    case 1:
        return ToolGroup::Line;
    case 2:
        return ToolGroup::Shape;
    case 0:
    default:
        return ToolGroup::None;
    }
}

ToolGroup PreferredGroupForTool(Tool tool, ToolGroup currentGroup) {
    if (IsLineGroupTool(tool)) {
        return ToolGroup::Line;
    }
    if (IsShapeGroupTool(tool)) {
        return ToolGroup::Shape;
    }
    return currentGroup;
}

// Mirrors the active annotation controls into process defaults and disk settings.
void RememberAnnotationStyle(const CaptureState& state) {
    g_defaultToolValue = static_cast<int>(state.tool);
    g_defaultOpenGroupValue = static_cast<int>(PreferredGroupForTool(state.tool, state.openGroup));
    g_defaultColorIndex = state.colorIndex;
    g_defaultThicknessIndex = state.thicknessIndex;
    SaveAnnotationStyleConfig();
}

COLORREF CurrentColor(const CaptureState& state) {
    return kPalette[ClampValue(state.colorIndex, 0, kColorCount - 1)];
}

bool IsLightColor(COLORREF color) {
    const int red = GetRValue(color);
    const int green = GetGValue(color);
    const int blue = GetBValue(color);
    return red * 299 + green * 587 + blue * 114 > 186000;
}

COLORREF ContrastColor(COLORREF color) {
    return IsLightColor(color) ? RGB(17, 24, 39) : RGB(255, 255, 255);
}

int CurrentThickness(const CaptureState& state) {
    return kThicknesses[ClampValue(state.thicknessIndex, 0, kThicknessCount - 1)];
}

HFONT CreateAnnotationFont() {
    return CreateFontW(
        -22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

HPEN CreateStrokePen(COLORREF color, int thickness) {
    LOGBRUSH brush = {};
    brush.lbStyle = BS_SOLID;
    brush.lbColor = color;
    return ExtCreatePen(
        PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
        static_cast<DWORD>(std::max(1, thickness)),
        &brush,
        0,
        NULL);
}

RECT MakeCenteredRect(int centerX, int centerY, int size) {
    const int half = size / 2;
    RECT rect = {centerX - half, centerY - half, centerX - half + size, centerY - half + size};
    return rect;
}

void GetSelectionHandleRects(const RECT& selection, RECT handles[8]) {
    const int centerX = (selection.left + selection.right) / 2;
    const int centerY = (selection.top + selection.bottom) / 2;
    handles[0] = MakeCenteredRect(selection.left, selection.top, kResizeHandleSize);
    handles[1] = MakeCenteredRect(centerX, selection.top, kResizeHandleSize);
    handles[2] = MakeCenteredRect(selection.right, selection.top, kResizeHandleSize);
    handles[3] = MakeCenteredRect(selection.right, centerY, kResizeHandleSize);
    handles[4] = MakeCenteredRect(selection.right, selection.bottom, kResizeHandleSize);
    handles[5] = MakeCenteredRect(centerX, selection.bottom, kResizeHandleSize);
    handles[6] = MakeCenteredRect(selection.left, selection.bottom, kResizeHandleSize);
    handles[7] = MakeCenteredRect(selection.left, centerY, kResizeHandleSize);
}

SelectionDragMode HandleModeFromIndex(int index) {
    static const SelectionDragMode modes[8] = {
        SelectionDragMode::ResizeTopLeft,
        SelectionDragMode::ResizeTop,
        SelectionDragMode::ResizeTopRight,
        SelectionDragMode::ResizeRight,
        SelectionDragMode::ResizeBottomRight,
        SelectionDragMode::ResizeBottom,
        SelectionDragMode::ResizeBottomLeft,
        SelectionDragMode::ResizeLeft
    };
    return index >= 0 && index < 8 ? modes[index] : SelectionDragMode::None;
}

SelectionDragMode HitTestSelectionDrag(const CaptureState& state, POINT point) {
    if (!state.hasSelection) {
        return SelectionDragMode::None;
    }

    RECT handles[8] = {};
    GetSelectionHandleRects(state.selection, handles);
    for (int index = 0; index < 8; ++index) {
        if (PtInRect(&handles[index], point)) {
            return HandleModeFromIndex(index);
        }
    }

    RECT outer = state.selection;
    InflateRect(&outer, kSelectionHitSlop, kSelectionHitSlop);
    RECT inner = state.selection;
    InflateRect(&inner, -kSelectionHitSlop, -kSelectionHitSlop);
    const bool inInner = RectWidth(inner) > 0 && RectHeight(inner) > 0 && PtInRect(&inner, point);
    if (PtInRect(&outer, point) && !inInner) {
        return SelectionDragMode::Move;
    }
    return SelectionDragMode::None;
}

HCURSOR CursorForSelectionDragMode(SelectionDragMode mode) {
    switch (mode) {
    case SelectionDragMode::Move:
        return LoadCursorW(NULL, IDC_SIZEALL);
    case SelectionDragMode::ResizeLeft:
    case SelectionDragMode::ResizeRight:
        return LoadCursorW(NULL, IDC_SIZEWE);
    case SelectionDragMode::ResizeTop:
    case SelectionDragMode::ResizeBottom:
        return LoadCursorW(NULL, IDC_SIZENS);
    case SelectionDragMode::ResizeTopLeft:
    case SelectionDragMode::ResizeBottomRight:
        return LoadCursorW(NULL, IDC_SIZENWSE);
    case SelectionDragMode::ResizeTopRight:
    case SelectionDragMode::ResizeBottomLeft:
        return LoadCursorW(NULL, IDC_SIZENESW);
    case SelectionDragMode::None:
    default:
        return NULL;
    }
}

RECT ClampMovedSelection(const CaptureState& state, RECT rect) {
    const int width = RectWidth(rect);
    const int height = RectHeight(rect);
    if (rect.left < 0) {
        OffsetRect(&rect, -rect.left, 0);
    }
    if (rect.top < 0) {
        OffsetRect(&rect, 0, -rect.top);
    }
    if (rect.right > state.width) {
        OffsetRect(&rect, state.width - rect.right, 0);
    }
    if (rect.bottom > state.height) {
        OffsetRect(&rect, 0, state.height - rect.bottom);
    }
    rect.right = rect.left + width;
    rect.bottom = rect.top + height;
    return rect;
}

RECT AdjustSelectionRect(const CaptureState& state, POINT point) {
    RECT rect = state.selectionDragOriginal;
    const int dx = point.x - state.selectionDragStart.x;
    const int dy = point.y - state.selectionDragStart.y;

    switch (state.selectionDragMode) {
    case SelectionDragMode::Move:
        OffsetRect(&rect, dx, dy);
        return ClampMovedSelection(state, rect);
    case SelectionDragMode::ResizeLeft:
        rect.left = ClampValue(rect.left + dx, 0L, rect.right - kMinSelectionSize);
        break;
    case SelectionDragMode::ResizeRight:
        rect.right = ClampValue(rect.right + dx, rect.left + kMinSelectionSize, static_cast<LONG>(state.width));
        break;
    case SelectionDragMode::ResizeTop:
        rect.top = ClampValue(rect.top + dy, 0L, rect.bottom - kMinSelectionSize);
        break;
    case SelectionDragMode::ResizeBottom:
        rect.bottom = ClampValue(rect.bottom + dy, rect.top + kMinSelectionSize, static_cast<LONG>(state.height));
        break;
    case SelectionDragMode::ResizeTopLeft:
        rect.left = ClampValue(rect.left + dx, 0L, rect.right - kMinSelectionSize);
        rect.top = ClampValue(rect.top + dy, 0L, rect.bottom - kMinSelectionSize);
        break;
    case SelectionDragMode::ResizeTopRight:
        rect.right = ClampValue(rect.right + dx, rect.left + kMinSelectionSize, static_cast<LONG>(state.width));
        rect.top = ClampValue(rect.top + dy, 0L, rect.bottom - kMinSelectionSize);
        break;
    case SelectionDragMode::ResizeBottomLeft:
        rect.left = ClampValue(rect.left + dx, 0L, rect.right - kMinSelectionSize);
        rect.bottom = ClampValue(rect.bottom + dy, rect.top + kMinSelectionSize, static_cast<LONG>(state.height));
        break;
    case SelectionDragMode::ResizeBottomRight:
        rect.right = ClampValue(rect.right + dx, rect.left + kMinSelectionSize, static_cast<LONG>(state.width));
        rect.bottom = ClampValue(rect.bottom + dy, rect.top + kMinSelectionSize, static_cast<LONG>(state.height));
        break;
    case SelectionDragMode::None:
    default:
        break;
    }
    return rect;
}

void OffsetAnnotations(CaptureState& state, int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    std::vector<Annotation>* groups[2] = {&state.annotations, &state.redoAnnotations};
    for (int groupIndex = 0; groupIndex < 2; ++groupIndex) {
        std::vector<Annotation>& annotations = *groups[groupIndex];
        for (std::size_t index = 0; index < annotations.size(); ++index) {
            annotations[index].start.x += dx;
            annotations[index].start.y += dy;
            annotations[index].end.x += dx;
            annotations[index].end.y += dy;
            for (std::size_t pointIndex = 0; pointIndex < annotations[index].points.size(); ++pointIndex) {
                annotations[index].points[pointIndex].x += dx;
                annotations[index].points[pointIndex].y += dy;
            }
        }
    }
    state.textAnchor.x += dx;
    state.textAnchor.y += dy;
}

Annotation CreateStyledAnnotation(
    const CaptureState& state,
    Tool tool,
    POINT start,
    POINT end,
    const std::wstring& text) {
    Annotation annotation;
    annotation.tool = tool;
    annotation.start = start;
    annotation.end = end;
    annotation.text = text;
    annotation.color = CurrentColor(state);
    annotation.thickness = CurrentThickness(state);
    if (tool == Tool::Doodle) {
        annotation.points = state.currentPoints;
        if (annotation.points.empty()) {
            annotation.points.push_back(start);
        }
        const POINT last = annotation.points.back();
        if (last.x != end.x || last.y != end.y) {
            annotation.points.push_back(end);
        }
    }
    return annotation;
}

void AddAnnotation(CaptureState& state, const Annotation& annotation) {
    state.annotations.push_back(annotation);
    state.redoAnnotations.clear();
}

bool UndoAnnotation(CaptureState& state) {
    if (state.annotations.empty()) {
        return false;
    }
    state.redoAnnotations.push_back(state.annotations.back());
    state.annotations.pop_back();
    return true;
}

bool RedoAnnotation(CaptureState& state) {
    if (state.redoAnnotations.empty()) {
        return false;
    }
    state.annotations.push_back(state.redoAnnotations.back());
    state.redoAnnotations.pop_back();
    return true;
}

POINT ClampTextEditorPosition(const CaptureState& state, int left, int top, int width, int height) {
    POINT position = {};
    const int maxLeft = std::max(static_cast<int>(state.selection.left), static_cast<int>(state.selection.right) - width);
    const int maxTop = std::max(static_cast<int>(state.selection.top), static_cast<int>(state.selection.bottom) - height);
    position.x = ClampValue(left, static_cast<int>(state.selection.left), maxLeft);
    position.y = ClampValue(top, static_cast<int>(state.selection.top), maxTop);
    return position;
}


void SelectDrawingObjects(HDC dc, HPEN pen, HBRUSH brush, HGDIOBJ& oldPen, HGDIOBJ& oldBrush) {
    oldPen = SelectObject(dc, pen);
    oldBrush = SelectObject(dc, brush);
}

void RestoreDrawingObjects(HDC dc, HGDIOBJ oldPen, HGDIOBJ oldBrush) {
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
}

// Renders one committed or preview annotation with its own color and thickness.
void DrawAnnotation(HDC dc, const Annotation& annotation, int offsetX, int offsetY) {
    const int x1 = annotation.start.x + offsetX;
    const int y1 = annotation.start.y + offsetY;
    const int x2 = annotation.end.x + offsetX;
    const int y2 = annotation.end.y + offsetY;

    HPEN pen = CreateStrokePen(annotation.color, annotation.thickness);
    if (pen == NULL) {
        return;
    }

    HGDIOBJ oldPen = NULL;
    HGDIOBJ oldBrush = NULL;
    SelectDrawingObjects(dc, pen, static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH)), oldPen, oldBrush);

    if (annotation.tool == Tool::Line) {
        MoveToEx(dc, x1, y1, NULL);
        LineTo(dc, x2, y2);
    } else if (annotation.tool == Tool::Arrow) {
        MoveToEx(dc, x1, y1, NULL);
        LineTo(dc, x2, y2);

        const double dx = static_cast<double>(x2 - x1);
        const double dy = static_cast<double>(y2 - y1);
        const double length = std::sqrt(dx * dx + dy * dy);
        if (length >= 1.0) {
            const double ux = dx / length;
            const double uy = dy / length;
            const double arrowLength = static_cast<double>(std::max(14, annotation.thickness * 4));
            const double arrowWidth = static_cast<double>(std::max(10, annotation.thickness * 3));
            POINT head[3] = {
                {x2, y2},
                {
                    static_cast<LONG>(x2 - ux * arrowLength - uy * arrowWidth * 0.5),
                    static_cast<LONG>(y2 - uy * arrowLength + ux * arrowWidth * 0.5)
                },
                {
                    static_cast<LONG>(x2 - ux * arrowLength + uy * arrowWidth * 0.5),
                    static_cast<LONG>(y2 - uy * arrowLength - ux * arrowWidth * 0.5)
                }
            };
            HBRUSH arrowBrush = CreateSolidBrush(annotation.color);
            HGDIOBJ oldArrowBrush = SelectObject(dc, arrowBrush);
            Polygon(dc, head, 3);
            SelectObject(dc, oldArrowBrush);
            DeleteObject(arrowBrush);
        }
    } else if (annotation.tool == Tool::RoundedRectangle) {
        const int radius = std::max(10, annotation.thickness * 3);
        RoundRect(
            dc,
            std::min(x1, x2),
            std::min(y1, y2),
            std::max(x1, x2),
            std::max(y1, y2),
            radius,
            radius);
    } else if (annotation.tool == Tool::Ellipse) {
        Ellipse(dc, std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2));
    } else if (annotation.tool == Tool::Doodle) {
        if (annotation.points.size() >= 2U) {
            std::vector<POINT> points = annotation.points;
            for (std::size_t index = 0; index < points.size(); ++index) {
                points[index].x += offsetX;
                points[index].y += offsetY;
            }
            Polyline(dc, points.data(), static_cast<int>(points.size()));
        } else {
            Ellipse(
                dc,
                x1 - annotation.thickness / 2,
                y1 - annotation.thickness / 2,
                x1 + annotation.thickness / 2,
                y1 + annotation.thickness / 2);
        }
    } else if (annotation.tool == Tool::Text && !annotation.text.empty()) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, annotation.color);
        HFONT font = CreateAnnotationFont();
        HGDIOBJ oldFont = NULL;
        if (font != NULL) {
            oldFont = SelectObject(dc, font);
        }
        SetTextColor(dc, ContrastColor(annotation.color));
        TextOutW(dc, x1 + 1, y1 + 1, annotation.text.c_str(), static_cast<int>(annotation.text.size()));
        SetTextColor(dc, annotation.color);
        TextOutW(dc, x1, y1, annotation.text.c_str(), static_cast<int>(annotation.text.size()));
        if (oldFont != NULL) {
            SelectObject(dc, oldFont);
        }
        if (font != NULL) {
            DeleteObject(font);
        }
    }

    RestoreDrawingObjects(dc, oldPen, oldBrush);
    DeleteObject(pen);
}
