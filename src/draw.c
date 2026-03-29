/*
 * draw.c  —  toolbar, image rendering, info overlay
 */
#include "../include/viewer.h"

/* ── Toolbar button layout ──────────────────────────────────────── */
typedef struct { int x; int w; int cmd; const WCHAR *label; const WCHAR *tip; } TBBtn;

static TBBtn s_btns[] = {
    {   4, 60, IDM_OPEN,       L"Open",     L"Open image (Ctrl+O)"    },
    {  68, 50, IDM_PREV,       L"◀ Prev",   L"Previous image (←)"     },
    { 122, 50, IDM_NEXT,       L"Next ▶",   L"Next image (→)"         },
    { 178, 46, IDM_FIT,        L"Fit",      L"Fit to window (F)"      },
    { 228, 46, IDM_RESET,      L"100%",     L"Reset zoom (R)"         },
    { 278, 50, IDM_ROTATE_CCW, L"↺ CCW",   L"Rotate left"            },
    { 332, 50, IDM_ROTATE_CW,  L"↻ CW",    L"Rotate right"           },
    { 386, 50, IDM_FLIP_H,     L"⇔ H",     L"Flip horizontal"        },
    { 440, 50, IDM_FLIP_V,     L"⇕ V",     L"Flip vertical"          },
    { 494, 80, IDM_SLIDESHOW,  L"▶ Slide",  L"Toggle slideshow (S)"   },
    { 578, 60, IDM_FULLSCREEN, L"⛶ Full",  L"Fullscreen (F11)"       },
    { 642, 60, IDM_INFO,       L"ℹ Info",   L"Image info (I)"         },
    { 706, 60, IDM_THUMB_TOGGLE,L"☰ Thumbs",L"Toggle thumbnails (T)"  },
};
#define BTN_COUNT (sizeof(s_btns)/sizeof(s_btns[0]))

/* ── Draw toolbar ───────────────────────────────────────────────── */
void draw_toolbar(HDC hdc, HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    /* Gradient background */
    TRIVERTEX tv[2] = {
        { 0,         0,         0x2200, 0x2200, 0x3000, 0 },
        { rc.right, TB_HEIGHT,  0x1800, 0x1800, 0x2500, 0 }
    };
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, tv, 2, &gr, 1, GRADIENT_FILL_RECT_H);

    /* Bottom border */
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 80, 120));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, 0, TB_HEIGHT - 1, NULL);
    LineTo(hdc, rc.right, TB_HEIGHT - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    /* Buttons */
    HFONT font = CreateFontW(13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < (int)BTN_COUNT; i++) {
        TBBtn *b = &s_btns[i];
        RECT br = { b->x, 4, b->x + b->w, TB_HEIGHT - 4 };

        /* Highlight slideshow button when active */
        BOOL active = (b->cmd == IDM_SLIDESHOW && g_app.slideshowOn) ||
                      (b->cmd == IDM_INFO       && g_app.infoVisible) ||
                      (b->cmd == IDM_THUMB_TOGGLE && g_app.thumbVisible);

        HBRUSH btnBr = CreateSolidBrush(active ? RGB(60,130,220) : RGB(45,55,75));
        HPEN   btnPen = CreatePen(PS_SOLID, 1, RGB(80,100,140));
        SelectObject(hdc, btnBr);
        SelectObject(hdc, btnPen);
        RoundRect(hdc, br.left, br.top, br.right, br.bottom, 6, 6);
        DeleteObject(btnBr);
        DeleteObject(btnPen);

        SetTextColor(hdc, RGB(210, 220, 255));
        DrawTextW(hdc, b->label, -1, &br, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* Zoom label on right side */
    if (g_app.imgW) {
        WCHAR zlbl[32];
        swprintf_s(zlbl, 32, L"%.0f%%", g_app.zoom * 100.0);
        RECT zrc = { rc.right - 80, 0, rc.right - 4, TB_HEIGHT };
        SetTextColor(hdc, RGB(150, 170, 220));
        DrawTextW(hdc, zlbl, -1, &zrc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldPen);
    DeleteObject(font);
}

/* ── Draw main image ────────────────────────────────────────────── */
void draw_image(HDC hdc, HWND hwnd) {
    RECT canvas = ui_canvas_rect(hwnd);

    /* Dark canvas background */
    HBRUSH bgBr = CreateSolidBrush(RGB(22, 22, 28));
    FillRect(hdc, &canvas, bgBr);
    DeleteObject(bgBr);

    if (!g_app.hBitmap || !g_app.imgW) {
        /* Drop hint */
        HFONT font = CreateFontW(24,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        HGDIOBJ oldf = SelectObject(hdc, font);
        SetTextColor(hdc, RGB(70, 75, 100));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc,
            L"Drop an image here  •  Ctrl+O to open  •  ← → to browse",
            -1, &canvas, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldf);
        DeleteObject(font);
        return;
    }

    int dw = (int)(g_app.imgW * g_app.zoom);
    int dh = (int)(g_app.imgH * g_app.zoom);

    HDC mdc = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(mdc, g_app.hBitmap);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, NULL);

    /* Clip to canvas so image doesn't paint over toolbar/thumbs */
    HRGN clip = CreateRectRgn(canvas.left, canvas.top, canvas.right, canvas.bottom);
    SelectClipRgn(hdc, clip);

    StretchBlt(hdc, g_app.panX, g_app.panY, dw, dh,
               mdc, 0, 0, g_app.imgW, g_app.imgH, SRCCOPY);

    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);
    SelectObject(mdc, old);
    DeleteDC(mdc);

    /* Thin border around image */
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 65, 90));
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, nullBr);
    Rectangle(hdc, g_app.panX, g_app.panY, g_app.panX + dw + 1, g_app.panY + dh + 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

/* ── Draw info overlay ──────────────────────────────────────────── */
void draw_info(HDC hdc, HWND hwnd) {
    if (!g_app.infoVisible || !g_app.imgW) return;

    RECT canvas = ui_canvas_rect(hwnd);
    /* Semi-transparent panel at bottom-left */
    RECT box = { canvas.left + 10, canvas.bottom - 110,
                 canvas.left + 340, canvas.bottom - 10 };

    /* Background */
    HBRUSH br = CreateSolidBrush(RGB(15, 15, 22));
    HPEN   p  = CreatePen(PS_SOLID, 1, RGB(80, 100, 160));
    SelectObject(hdc, br);
    SelectObject(hdc, p);
    RoundRect(hdc, box.left, box.top, box.right, box.bottom, 8, 8);
    DeleteObject(br);
    DeleteObject(p);

    HFONT font = CreateFontW(14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Consolas");
    HGDIOBJ oldf = SelectObject(hdc, font);
    SetTextColor(hdc, RGB(180, 210, 255));
    SetBkMode(hdc, TRANSPARENT);
    RECT txt = { box.left + 10, box.top + 8, box.right - 8, box.bottom - 8 };
    DrawTextW(hdc, g_app.infoText, -1, &txt, DT_LEFT | DT_TOP);
    SelectObject(hdc, oldf);
    DeleteObject(font);
}

/* ── Full scene repaint ─────────────────────────────────────────── */
void draw_scene(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    HDC hdc    = GetDC(hwnd);
    HDC memDC  = CreateCompatibleDC(hdc);
    HBITMAP buf = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(memDC, buf);

    draw_image(memDC, hwnd);
    draw_toolbar(memDC, hwnd);
    if (g_app.thumbVisible) thumbs_draw(memDC, rc);
    draw_info(memDC, hwnd);

    /* Blit double-buffer to screen */
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, old);
    DeleteObject(buf);
    DeleteDC(memDC);
    ReleaseDC(hwnd, hdc);
}
