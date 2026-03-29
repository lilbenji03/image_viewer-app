/*
 * draw.c  —  toolbar, image rendering, info overlay, scene composite
 *
 * Improvements over v2:
 *  - g_btns[] and g_btnCount are extern-accessible (shared with main.c)
 *    so toolbar hit-testing uses the same table — no duplication
 *  - toolbar_hit() lives here so main.c just calls it
 *  - Zoom % removed from toolbar right side (it's in the title bar)
 *  - Filename displayed in toolbar right area instead
 *  - Info panel is now larger (5 rows), positioned top-left to avoid
 *    blocking the image centre
 *  - draw_checkerboard() shows transparency pattern behind images with alpha
 */
#include "../include/viewer.h"

/* ════════════════════════════════════════════════════════════════════
   Toolbar button table  (shared via extern in viewer.h)
   ════════════════════════════════════════════════════════════════════ */
TBBtn g_btns[] = {
    {   4,  60, IDM_OPEN,        L"Open",      L"Open image (Ctrl+O)"     },
    {  68,  50, IDM_PREV,        L"\u25C4 Prev",L"Previous image (\u2190)" },
    { 122,  50, IDM_NEXT,        L"Next \u25BA",L"Next image (\u2192)"     },
    { 178,  46, IDM_FIT,         L"Fit",        L"Fit to window (F)"       },
    { 228,  46, IDM_RESET,       L"100%",       L"Reset zoom (R)"          },
    { 278,  50, IDM_ROTATE_CCW,  L"\u21BA CCW", L"Rotate left"             },
    { 332,  50, IDM_ROTATE_CW,   L"\u21BB CW",  L"Rotate right"            },
    { 386,  50, IDM_FLIP_H,      L"\u21D4 H",   L"Flip horizontal"         },
    { 440,  50, IDM_FLIP_V,      L"\u21D5 V",   L"Flip vertical"           },
    { 494,  80, IDM_SLIDESHOW,   L"\u25B6 Slide",L"Toggle slideshow (S)"   },
    { 578,  60, IDM_FULLSCREEN,  L"\u26F6 Full", L"Fullscreen (F11)"        },
    { 642,  60, IDM_INFO,        L"\u24D8 Info", L"Image info (I)"          },
    { 706,  60, IDM_THUMB_TOGGLE,L"\u2630 Thumbs",L"Toggle thumbnails (T)" },
};
int g_btnCount = (int)(sizeof(g_btns) / sizeof(g_btns[0]));

/* ── Toolbar hit-test (used by main.c WM_LBUTTONDOWN) ───────────── */
int toolbar_hit(int mx, int my) {
    if (my < 0 || my >= TB_HEIGHT) return -1;
    for (int i = 0; i < g_btnCount; i++)
        if (mx >= g_btns[i].x && mx < g_btns[i].x + g_btns[i].w)
            return g_btns[i].cmd;
    return -1;
}

/* ── Draw toolbar ───────────────────────────────────────────────── */
void draw_toolbar(HDC hdc, HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);

    /* Gradient background */
    TRIVERTEX tv[2] = {
        { 0,        0,         0x2200, 0x2200, 0x3000, 0 },
        { rc.right, TB_HEIGHT, 0x1800, 0x1800, 0x2500, 0 }
    };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_H);

    /* Bottom border line */
    HPEN pen    = CreatePen(PS_SOLID, 1, RGB(60, 80, 120));
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, 0,        TB_HEIGHT - 1, NULL);
    LineTo  (hdc, rc.right, TB_HEIGHT - 1);
    SelectObject(hdc, old);
    DeleteObject(pen);

    /* Button font */
    HFONT font = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < g_btnCount; i++) {
        TBBtn *b  = &g_btns[i];
        RECT   br = { b->x, 4, b->x + b->w, TB_HEIGHT - 4 };

        BOOL active = (b->cmd == IDM_SLIDESHOW    && g_app.slideshowOn)
                   || (b->cmd == IDM_INFO         && g_app.infoVisible)
                   || (b->cmd == IDM_THUMB_TOGGLE && g_app.thumbVisible);

        COLORREF btnCol = active ? RGB(55, 125, 215) : RGB(42, 52, 72);
        HBRUSH   btnBr  = CreateSolidBrush(btnCol);
        HPEN     btnPen = CreatePen(PS_SOLID, 1, RGB(75, 95, 135));
        SelectObject(hdc, btnBr);
        SelectObject(hdc, btnPen);
        RoundRect(hdc, br.left, br.top, br.right, br.bottom, 6, 6);
        DeleteObject(btnBr);
        DeleteObject(btnPen);

        SetTextColor(hdc, active ? RGB(255, 255, 255) : RGB(205, 215, 255));
        DrawTextW(hdc, b->label, -1, &br,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* Filename on the right side of the toolbar */
    if (g_app.imgW) {
        const WCHAR *slash = wcsrchr(g_app.curPath, L'\\');
        const WCHAR *name  = slash ? slash + 1 : g_app.curPath;
        /* Strip extension for compactness */
        WCHAR shortname[MAX_PATH];
        wcscpy_s(shortname, MAX_PATH, name);
        WCHAR *dot = wcsrchr(shortname, L'.');
        if (dot) *dot = 0;

        RECT nrc = { 775, 0, rc.right - 6, TB_HEIGHT };
        SetTextColor(hdc, RGB(140, 160, 210));
        DrawTextW(hdc, shortname, -1, &nrc,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

/* ── Draw checkerboard pattern (indicates transparency) ─────────── */
static void draw_checkerboard(HDC hdc, RECT r) {
    int sz = 10;
    for (int y = r.top; y < r.bottom; y += sz) {
        for (int x = r.left; x < r.right; x += sz) {
            BOOL light = (((x - r.left) / sz) + ((y - r.top) / sz)) % 2 == 0;
            RECT cell = { x, y,
                          min(x + sz, r.right),
                          min(y + sz, r.bottom) };
            HBRUSH br = CreateSolidBrush(light ? RGB(200, 200, 200)
                                               : RGB(160, 160, 160));
            FillRect(hdc, &cell, br);
            DeleteObject(br);
        }
    }
}

/* ── Draw the main image ────────────────────────────────────────── */
void draw_image(HDC hdc, HWND hwnd) {
    RECT canvas = ui_canvas_rect(hwnd);

    /* Canvas background */
    HBRUSH bgBr = CreateSolidBrush(RGB(20, 20, 26));
    FillRect(hdc, &canvas, bgBr);
    DeleteObject(bgBr);

    if (!g_app.hBitmap || !g_app.imgW) {
        /* Drop hint text */
        HFONT font = CreateFontW(22, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ oldf = SelectObject(hdc, font);
        SetTextColor(hdc, RGB(65, 70, 95));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc,
            L"Drop an image here  \u2022  Ctrl+O to open  \u2022  \u2190 \u2192 to browse",
            -1, &canvas, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldf);
        DeleteObject(font);
        return;
    }

    int dw = (int)((double)g_app.imgW * g_app.zoom);
    int dh = (int)((double)g_app.imgH * g_app.zoom);

    /* Checkerboard behind image (visible for PNG with transparency) */
    RECT imgRect = { g_app.panX, g_app.panY,
                     g_app.panX + dw, g_app.panY + dh };
    RECT visible;
    if (IntersectRect(&visible, &imgRect, &canvas))
        draw_checkerboard(hdc, visible);

    /* Clip to canvas */
    HRGN clip = CreateRectRgn(canvas.left, canvas.top,
                               canvas.right, canvas.bottom);
    SelectClipRgn(hdc, clip);

    HDC mdc = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mdc, g_app.hBitmap);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);
    StretchBlt(hdc, g_app.panX, g_app.panY, dw, dh,
               mdc, 0, 0, g_app.imgW, g_app.imgH, SRCCOPY);
    SelectObject(mdc, old);
    DeleteDC(mdc);

    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);

    /* Thin border around image */
    HPEN   bpen  = CreatePen(PS_SOLID, 1, RGB(55, 60, 85));
    HBRUSH nullB = (HBRUSH)GetStockObject(NULL_BRUSH);
    HGDIOBJ op   = SelectObject(hdc, bpen);
    SelectObject(hdc, nullB);
    Rectangle(hdc, g_app.panX, g_app.panY,
              g_app.panX + dw + 1, g_app.panY + dh + 1);
    SelectObject(hdc, op);
    DeleteObject(bpen);
}

/* ── Draw info overlay ──────────────────────────────────────────── */
void draw_info(HDC hdc, HWND hwnd) {
    if (!g_app.infoVisible || !g_app.imgW) return;

    RECT canvas = ui_canvas_rect(hwnd);
    RECT box = { canvas.left + 10, canvas.top + 10,
                 canvas.left + 360, canvas.top + 120 };

    HBRUSH br = CreateSolidBrush(RGB(12, 12, 20));
    HPEN   p  = CreatePen(PS_SOLID, 1, RGB(75, 95, 155));
    SelectObject(hdc, br);
    SelectObject(hdc, p);
    RoundRect(hdc, box.left, box.top, box.right, box.bottom, 10, 10);
    DeleteObject(br);
    DeleteObject(p);

    HFONT font = CreateFontW(13, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Consolas");
    HGDIOBJ oldf = SelectObject(hdc, font);
    SetTextColor(hdc, RGB(170, 205, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT txt = { box.left + 12, box.top + 10,
                 box.right - 10, box.bottom - 8 };
    DrawTextW(hdc, g_app.infoText, -1, &txt, DT_LEFT | DT_TOP);
    SelectObject(hdc, oldf);
    DeleteObject(font);
}

/* ── Full scene repaint (double-buffered) ───────────────────────── */
void draw_scene(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    HDC      hdc   = GetDC(hwnd);
    HDC      memDC = CreateCompatibleDC(hdc);
    HBITMAP  buf   = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ  old   = SelectObject(memDC, buf);

    draw_image  (memDC, hwnd);
    draw_toolbar(memDC, hwnd);
    if (g_app.thumbVisible) thumbs_draw(memDC, rc);
    draw_info   (memDC, hwnd);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, old);
    DeleteObject(buf);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);
}
