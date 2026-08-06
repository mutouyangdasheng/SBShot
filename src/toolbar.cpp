// SBShot
// Author: li xiaojun
// Module: toolbar

struct ToolbarLayout {
    RECT toolButtons[kToolbarButtonCount];
    RECT lineOptionButtons[2];
    RECT shapeOptionButtons[2];
    RECT colorSwatches[kColorCount];
    RECT thicknessButtons[kThicknessCount];
};

// Computes stable toolbar, submenu, color, and thickness hit rectangles.
void GetToolbarLayout(const CaptureState& state, ToolbarLayout& layout) {
    const int buttonWidth = 38;
    const int buttonHeight = 24;
    const int buttonGap = 6;
    const int swatchSize = 22;
    const int swatchGap = 5;
    const int thicknessWidth = 34;
    const int thicknessHeight = 22;
    const int styleGap = 14;
    const int styleRowHeight = 28;
    const bool showLineOptions = state.openGroup == ToolGroup::Line;
    const bool showShapeOptions = state.openGroup == ToolGroup::Shape;
    const bool showOptions = showLineOptions || showShapeOptions;
    const int optionRowHeight = showOptions ? buttonHeight + 6 : 0;
    const int totalHeight = buttonHeight + optionRowHeight + 8 + styleRowHeight;
    const int toolWidth = kToolbarButtonCount * buttonWidth + (kToolbarButtonCount - 1) * buttonGap;
    const int optionWidth = 2 * buttonWidth + buttonGap;
    const int styleWidth =
        kColorCount * swatchSize +
        (kColorCount - 1) * swatchGap +
        styleGap +
        kThicknessCount * thicknessWidth +
        (kThicknessCount - 1) * swatchGap;
    const int totalWidth = std::max(std::max(toolWidth, optionWidth), styleWidth);
    int left = state.selection.left;
    if (left + totalWidth > state.width) {
        left = state.width - totalWidth;
    }
    left = std::max(0, left);

    int top = state.selection.bottom + 8;
    if (top + totalHeight > state.height) {
        top = state.selection.top - totalHeight - 8;
    }
    top = ClampValue(top, 0, std::max(0, state.height - totalHeight));

    const int toolLeft = left + (totalWidth - toolWidth) / 2;
    for (int index = 0; index < kToolbarButtonCount; ++index) {
        layout.toolButtons[index].left = toolLeft + index * (buttonWidth + buttonGap);
        layout.toolButtons[index].top = top;
        layout.toolButtons[index].right = layout.toolButtons[index].left + buttonWidth;
        layout.toolButtons[index].bottom = top + buttonHeight;
    }

    const int optionTop = top + buttonHeight + 6;
    const int optionLeft = left + (totalWidth - optionWidth) / 2;
    for (int index = 0; index < 2; ++index) {
        layout.lineOptionButtons[index].left = showLineOptions ? optionLeft + index * (buttonWidth + buttonGap) : 0;
        layout.lineOptionButtons[index].top = showLineOptions ? optionTop : 0;
        layout.lineOptionButtons[index].right = showLineOptions ? layout.lineOptionButtons[index].left + buttonWidth : 0;
        layout.lineOptionButtons[index].bottom = showLineOptions ? optionTop + buttonHeight : 0;
        layout.shapeOptionButtons[index].left = showShapeOptions ? optionLeft + index * (buttonWidth + buttonGap) : 0;
        layout.shapeOptionButtons[index].top = showShapeOptions ? optionTop : 0;
        layout.shapeOptionButtons[index].right = showShapeOptions ? layout.shapeOptionButtons[index].left + buttonWidth : 0;
        layout.shapeOptionButtons[index].bottom = showShapeOptions ? optionTop + buttonHeight : 0;
    }

    const int styleTop = top + buttonHeight + optionRowHeight + 8;
    int cursor = left + (totalWidth - styleWidth) / 2;
    for (int index = 0; index < kColorCount; ++index) {
        layout.colorSwatches[index].left = cursor;
        layout.colorSwatches[index].top = styleTop + 3;
        layout.colorSwatches[index].right = cursor + swatchSize;
        layout.colorSwatches[index].bottom = styleTop + 3 + swatchSize;
        cursor += swatchSize + swatchGap;
    }

    cursor += styleGap;
    for (int index = 0; index < kThicknessCount; ++index) {
        layout.thicknessButtons[index].left = cursor;
        layout.thicknessButtons[index].top = styleTop + 3;
        layout.thicknessButtons[index].right = cursor + thicknessWidth;
        layout.thicknessButtons[index].bottom = styleTop + 3 + thicknessHeight;
        cursor += thicknessWidth + swatchGap;
    }
}

int ToolbarHitTest(const CaptureState& state, POINT point) {
    ToolbarLayout layout = {};
    GetToolbarLayout(state, layout);
    for (int index = 0; index < kToolbarButtonCount; ++index) {
        if (PtInRect(&layout.toolButtons[index], point)) {
            return index;
        }
    }
    if (state.openGroup == ToolGroup::Line) {
        for (int index = 0; index < 2; ++index) {
            if (PtInRect(&layout.lineOptionButtons[index], point)) {
                return kToolbarLineOptionActionBase + index;
            }
        }
    }
    if (state.openGroup == ToolGroup::Shape) {
        for (int index = 0; index < 2; ++index) {
            if (PtInRect(&layout.shapeOptionButtons[index], point)) {
                return kToolbarShapeOptionActionBase + index;
            }
        }
    }
    for (int index = 0; index < kColorCount; ++index) {
        if (PtInRect(&layout.colorSwatches[index], point)) {
            return kToolbarColorActionBase + index;
        }
    }
    for (int index = 0; index < kThicknessCount; ++index) {
        if (PtInRect(&layout.thicknessButtons[index], point)) {
            return kToolbarThicknessActionBase + index;
        }
    }
    return -1;
}

void FillRoundRect(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor, int radius) {
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, borderColor);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

RECT IconSquare(const RECT& rect) {
    const int size = std::max(12, std::min(RectWidth(rect), RectHeight(rect)) - 6);
    const int left = (rect.left + rect.right - size) / 2;
    const int top = (rect.top + rect.bottom - size) / 2;
    RECT icon = {left, top, left + size, top + size};
    return icon;
}

void DrawIconLine(HDC dc, const RECT& rect, bool arrow) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int x1 = icon.left + 1;
    const int y1 = icon.bottom - 2;
    const int x2 = icon.right - 1;
    const int y2 = icon.top + 2;
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    if (arrow) {
        MoveToEx(dc, x2, y2, NULL);
        LineTo(dc, x2 - 8, y2 + 1);
        MoveToEx(dc, x2, y2, NULL);
        LineTo(dc, x2 - 2, y2 + 8);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconShape(HDC dc, const RECT& rect, bool ellipse) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    if (ellipse) {
        RECT shape = {icon.left + 2, icon.top + 2, icon.right - 2, icon.bottom - 2};
        Ellipse(dc, shape.left, shape.top, shape.right, shape.bottom);
    } else {
        RECT shape = {icon.left + 2, icon.top + 2, icon.right - 2, icon.bottom - 2};
        RoundRect(dc, shape.left, shape.top, shape.right, shape.bottom, 6, 6);
    }
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconShapeGroup(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    POINT triangle[4] = {
        {icon.left + 2, icon.bottom - 2},
        {icon.left + 8, icon.top + 2},
        {icon.left + 14, icon.bottom - 2},
        {icon.left + 2, icon.bottom - 2}
    };
    RECT circle = {icon.right - 9, icon.top + 2, icon.right - 1, icon.top + 10};

    Polyline(dc, triangle, 4);
    Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
    MoveToEx(dc, icon.left + 11, icon.bottom - 5, NULL);
    LineTo(dc, icon.right - 1, icon.bottom - 5);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconDoodle(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    POINT points[6] = {
        {icon.left + 1, icon.bottom - 4},
        {icon.left + 4, icon.top + 6},
        {icon.left + 8, icon.bottom - 3},
        {icon.left + 12, icon.top + 4},
        {icon.right - 4, icon.bottom - 5},
        {icon.right - 1, icon.top + 7}
    };
    Polyline(dc, points, 6);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconText(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(222, 226, 232));
    HFONT font = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font != NULL ? font : GetStockObject(DEFAULT_GUI_FONT));
    SetTextAlign(dc, TA_CENTER | TA_BASELINE);
    TextOutW(dc, (icon.left + icon.right) / 2, icon.bottom - 1, L"T", 1);
    SetTextAlign(dc, TA_LEFT | TA_TOP);
    SelectObject(dc, oldFont);
    if (font != NULL) {
        DeleteObject(font);
    }
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 1);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, icon.left + 3, icon.bottom - 2, NULL);
    LineTo(dc, icon.right - 3, icon.bottom - 2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconComplete(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, icon.left + 2, icon.top + RectHeight(icon) / 2 + 1, NULL);
    LineTo(dc, icon.left + RectWidth(icon) / 3, icon.bottom - 3);
    LineTo(dc, icon.right - 1, icon.top + 3);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconSave(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RECT file = {icon.left + 3, icon.top + 1, icon.right - 3, icon.bottom - 1};
    MoveToEx(dc, file.left, file.top, NULL);
    LineTo(dc, file.right - 5, file.top);
    LineTo(dc, file.right, file.top + 5);
    LineTo(dc, file.right, file.bottom);
    LineTo(dc, file.left, file.bottom);
    LineTo(dc, file.left, file.top);
    MoveToEx(dc, file.right - 5, file.top, NULL);
    LineTo(dc, file.right - 5, file.top + 5);
    LineTo(dc, file.right, file.top + 5);
    MoveToEx(dc, icon.left + RectWidth(icon) / 2, icon.top + 5, NULL);
    LineTo(dc, icon.left + RectWidth(icon) / 2, icon.bottom - 6);
    MoveToEx(dc, icon.left + RectWidth(icon) / 2 - 4, icon.bottom - 10, NULL);
    LineTo(dc, icon.left + RectWidth(icon) / 2, icon.bottom - 6);
    LineTo(dc, icon.left + RectWidth(icon) / 2 + 4, icon.bottom - 10);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconCancel(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, icon.left + 3, icon.top + 3, NULL);
    LineTo(dc, icon.right - 3, icon.bottom - 3);
    MoveToEx(dc, icon.right - 3, icon.top + 3, NULL);
    LineTo(dc, icon.left + 3, icon.bottom - 3);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconPinImage(HDC dc, const RECT& rect) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, icon.left + 2, icon.top + 3, icon.right - 3, icon.bottom - 2);
    MoveToEx(dc, icon.left + 4, icon.bottom - 5, NULL);
    LineTo(dc, icon.left + 8, icon.bottom - 9);
    LineTo(dc, icon.left + 11, icon.bottom - 6);
    LineTo(dc, icon.right - 5, icon.top + 8);
    Ellipse(dc, icon.right - 8, icon.top + 5, icon.right - 4, icon.top + 9);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void DrawIconUndoRedo(HDC dc, const RECT& rect, bool redo) {
    RECT icon = IconSquare(rect);
    HPEN pen = CreateStrokePen(RGB(222, 226, 232), 2);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    const int left = icon.left + 2;
    const int top = icon.top + 3;
    const int right = icon.right - 2;
    const int bottom = icon.bottom - 3;
    const int midY = top + 5;
    if (redo) {
        MoveToEx(dc, left, bottom, NULL);
        LineTo(dc, left, midY);
        LineTo(dc, right - 1, midY);
        MoveToEx(dc, right - 1, midY, NULL);
        LineTo(dc, right - 6, top);
        MoveToEx(dc, right - 1, midY, NULL);
        LineTo(dc, right - 6, midY + 5);
    } else {
        MoveToEx(dc, right, bottom, NULL);
        LineTo(dc, right, midY);
        LineTo(dc, left + 1, midY);
        MoveToEx(dc, left + 1, midY, NULL);
        LineTo(dc, left + 6, top);
        MoveToEx(dc, left + 1, midY, NULL);
        LineTo(dc, left + 6, midY + 5);
    }
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

const wchar_t* ToolbarTooltipText(int action) {
    static const wchar_t* colorNames[kColorCount] = {
        L"红色", L"橙色", L"黄色", L"绿色", L"蓝色", L"紫色", L"白色", L"黑色"
    };
    if (action == 0) {
        return L"撤销";
    }
    if (action == 1) {
        return L"重做";
    }
    if (action == 2) {
        return L"线条";
    }
    if (action == 3) {
        return L"形状";
    }
    if (action == 4) {
        return L"涂鸦";
    }
    if (action == 5) {
        return L"文字";
    }
    if (action == 6) {
        return L"完成";
    }
    if (action == 7) {
        return L"取消";
    }
    if (action == 8) {
        return L"贴图";
    }
    if (action == 9) {
        return L"保存";
    }
    if (action == kToolbarLineOptionActionBase) {
        return L"直线";
    }
    if (action == kToolbarLineOptionActionBase + 1) {
        return L"箭头";
    }
    if (action == kToolbarShapeOptionActionBase) {
        return L"圆角方框";
    }
    if (action == kToolbarShapeOptionActionBase + 1) {
        return L"圆形";
    }
    if (action >= kToolbarColorActionBase && action < kToolbarColorActionBase + kColorCount) {
        return colorNames[action - kToolbarColorActionBase];
    }
    if (action >= kToolbarThicknessActionBase && action < kToolbarThicknessActionBase + kThicknessCount) {
        static const wchar_t* thicknessNames[kThicknessCount] = {L"细", L"中", L"粗", L"特粗"};
        return thicknessNames[action - kToolbarThicknessActionBase];
    }
    return L"";
}

void DrawToolbarButton(HDC dc, const RECT& rect, bool selected, bool hovered, COLORREF accent, int action) {
    COLORREF background = hovered ? RGB(49, 57, 70) : RGB(31, 36, 46);
    COLORREF border = RGB(73, 84, 100);
    if (selected) {
        background = accent;
        border = RGB(209, 218, 230);
    }
    FillRoundRect(dc, rect, background, border, 8);
    if (action == 0) {
        DrawIconUndoRedo(dc, rect, false);
    } else if (action == 1) {
        DrawIconUndoRedo(dc, rect, true);
    } else if (action == 2) {
        DrawIconLine(dc, rect, false);
    } else if (action == 3) {
        DrawIconShapeGroup(dc, rect);
    } else if (action == 4) {
        DrawIconDoodle(dc, rect);
    } else if (action == 5) {
        DrawIconText(dc, rect);
    } else if (action == 6) {
        DrawIconComplete(dc, rect);
    } else if (action == 7) {
        DrawIconCancel(dc, rect);
    } else if (action == 8) {
        DrawIconPinImage(dc, rect);
    } else if (action == 9) {
        DrawIconSave(dc, rect);
    } else if (action == kToolbarLineOptionActionBase) {
        DrawIconLine(dc, rect, false);
    } else if (action == kToolbarLineOptionActionBase + 1) {
        DrawIconLine(dc, rect, true);
    } else if (action == kToolbarShapeOptionActionBase) {
        DrawIconShape(dc, rect, false);
    } else if (action == kToolbarShapeOptionActionBase + 1) {
        DrawIconShape(dc, rect, true);
    }
}

void DrawToolbarTooltip(HDC dc, const CaptureState& state) {
    if (state.hoverAction < 0) {
        return;
    }
    const wchar_t* text = ToolbarTooltipText(state.hoverAction);
    if (text[0] == L'\0') {
        return;
    }

    HGDIOBJ oldFont = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SIZE textSize = {};
    GetTextExtentPoint32W(dc, text, static_cast<int>(std::wcslen(text)), &textSize);
    RECT tip = {
        state.hoverPoint.x + 12,
        state.hoverPoint.y + 16,
        state.hoverPoint.x + 12 + textSize.cx + 16,
        state.hoverPoint.y + 16 + textSize.cy + 12
    };
    if (tip.right > state.width) {
        OffsetRect(&tip, state.width - tip.right - 4, 0);
    }
    if (tip.bottom > state.height) {
        OffsetRect(&tip, 0, -RectHeight(tip) - 30);
    }
    FillRoundRect(dc, tip, RGB(17, 24, 39), RGB(75, 85, 99), 6);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(249, 250, 251));
    TextOutW(dc, tip.left + 8, tip.top + 6, text, static_cast<int>(std::wcslen(text)));
    SelectObject(dc, oldFont);
}

// Draws the compact icon-only annotation toolbar and hover tooltip.
void DrawToolbar(HDC dc, const CaptureState& state) {
    ToolbarLayout layout = {};
    GetToolbarLayout(state, layout);

    const bool selected[kToolbarButtonCount] = {
        false,
        false,
        IsLineGroupTool(state.tool),
        IsShapeGroupTool(state.tool),
        state.tool == Tool::Doodle,
        state.tool == Tool::Text,
        false,
        false,
        false,
        false
    };
    const COLORREF accents[kToolbarButtonCount] = {
        RGB(75, 85, 99),
        RGB(75, 85, 99),
        RGB(37, 99, 235),
        RGB(37, 99, 235),
        RGB(37, 99, 235),
        RGB(37, 99, 235),
        RGB(22, 163, 74),
        RGB(220, 38, 38),
        RGB(14, 165, 233),
        RGB(124, 58, 237)
    };

    for (int index = 0; index < kToolbarButtonCount; ++index) {
        DrawToolbarButton(
            dc,
            layout.toolButtons[index],
            selected[index],
            state.hoverAction == index,
            accents[index],
            index);
    }

    if (state.openGroup == ToolGroup::Line) {
        DrawToolbarButton(
            dc,
            layout.lineOptionButtons[0],
            state.tool == Tool::Line,
            state.hoverAction == kToolbarLineOptionActionBase,
            RGB(37, 99, 235),
            kToolbarLineOptionActionBase);
        DrawToolbarButton(
            dc,
            layout.lineOptionButtons[1],
            state.tool == Tool::Arrow,
            state.hoverAction == kToolbarLineOptionActionBase + 1,
            RGB(37, 99, 235),
            kToolbarLineOptionActionBase + 1);
    } else if (state.openGroup == ToolGroup::Shape) {
        DrawToolbarButton(
            dc,
            layout.shapeOptionButtons[0],
            state.tool == Tool::RoundedRectangle,
            state.hoverAction == kToolbarShapeOptionActionBase,
            RGB(37, 99, 235),
            kToolbarShapeOptionActionBase);
        DrawToolbarButton(
            dc,
            layout.shapeOptionButtons[1],
            state.tool == Tool::Ellipse,
            state.hoverAction == kToolbarShapeOptionActionBase + 1,
            RGB(37, 99, 235),
            kToolbarShapeOptionActionBase + 1);
    }

    for (int index = 0; index < kColorCount; ++index) {
        RECT swatch = layout.colorSwatches[index];
        if (state.hoverAction == kToolbarColorActionBase + index) {
            RECT hover = swatch;
            InflateRect(&hover, 3, 3);
            FillRoundRect(dc, hover, RGB(147, 197, 253), RGB(147, 197, 253), 10);
        }
        FillRoundRect(
            dc,
            swatch,
            kPalette[index],
            index == state.colorIndex ? RGB(249, 250, 251) : RGB(75, 85, 99),
            8);
    }

    for (int index = 0; index < kThicknessCount; ++index) {
        const int action = kToolbarThicknessActionBase + index;
        FillRoundRect(
            dc,
            layout.thicknessButtons[index],
            index == state.thicknessIndex ? RGB(37, 99, 235) :
                (state.hoverAction == action ? RGB(49, 57, 70) : RGB(31, 36, 46)),
            RGB(75, 85, 99),
            7);
        HPEN previewPen = CreateStrokePen(RGB(245, 247, 250), kThicknesses[index]);
        HGDIOBJ oldPreviewPen = SelectObject(dc, previewPen);
        const int midY = (layout.thicknessButtons[index].top + layout.thicknessButtons[index].bottom) / 2;
        MoveToEx(dc, layout.thicknessButtons[index].left + 8, midY, NULL);
        LineTo(dc, layout.thicknessButtons[index].right - 8, midY);
        SelectObject(dc, oldPreviewPen);
        DeleteObject(previewPen);
    }

    DrawToolbarTooltip(dc, state);
}
