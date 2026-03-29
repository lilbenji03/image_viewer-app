/*
 * thumbs.c  —  thumbnail sidebar
 *
 * Improvements over v2:
 *  - thumbs array is heap-allocated (tracks dirFiles capacity)
 *  - thumbs_sync(): re-syncs thumb array when folder changes
 *  - thumbs_load_visible(): lazy-loads only what's on screen
 *  - Visual scrollbar drawn on right edge of panel
 *  - Auto-scroll to current image when it changes
 *  - thumbs_on_scroll() receives clientRC to compute proper max scroll
 */
#include "../include/viewer.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

#define ITEM_H  (THUMB_H + 20)   /* thumb image + filename label      */

/* ── Build a thumbnail HBITMAP from file ────────────────────────── */
static HBITMAP make_thumb(HDC hdc, const WCHAR *wpath,
                          int *out_w, int *out_h) {
    char utf8[MAX_PATH * 4];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                        utf8, (int)sizeof(utf8), NULL, NULL);

    int w, h, ch;
    uint8_t *data = stbi_load(utf8, &w, &h, &ch, 4);
    if (!data) return NULL;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    /* Preserve aspect ratio within thumbnail cell */
    int tw = THUMB_WIDTH  - THUMB_PADDING * 2;
    int th = THUMB_H      - THUMB_PADDING * 2;
    float sx = (float)tw / (float)w;
    float sy = (float)th / (float)h;
    float s  = sx < sy ? sx : sy;
    int dw = (int)((float)w * s); if (dw < 1) dw = 1;
    int dh = (int)((float)h * s); if (dh < 1) dh = 1;

    uint8_t *resized = (uint8_t *)malloc((size_t)dw * dh * 4);
    if (!resized) { stbi_image_free(data); return NULL; }

    stbir_resize_uint8_linear(data, w, h, 0,
                              resized, dw, dh, 0, STBIR_RGBA);
    stbi_image_free(data);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       =  dw;
    bmi.bmiHeader.biHeight      = -dh;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hbm && bits) {
        uint8_t *dst = (uint8_t *)bits;
        for (int i = 0; i < dw * dh; i++) {
            dst[i*4+0] = resized[i*4+2];
            dst[i*4+1] = resized[i*4+1];
            dst[i*4+2] = resized[i*4+0];
            dst[i*4+3] = resized[i*4+3];
        }
    }
    free(resized);
    return hbm;
}

/* ── (Re-)sync thumb array length to dirFiles ───────────────────── */
void thumbs_sync(void) {
    /* Free old array if capacity mismatch */
    if (g_app.thumbs && g_app.thumbCount != g_app.dirCount) {
        thumbs_free_all();
    }
    if (!g_app.thumbs && g_app.dirCount > 0) {
        g_app.thumbs = (Thumb *)calloc((size_t)g_app.dirCount, sizeof(Thumb));
    }
    if (!g_app.thumbs) return;

    g_app.thumbCount = g_app.dirCount;
    for (int i = 0; i < g_app.dirCount; i++) {
        if (g_app.thumbs[i].hbm == NULL)   /* preserve cached bitmaps */
            wcscpy_s(g_app.thumbs[i].path, MAX_PATH, g_app.dirFiles[i]);
    }
}

/* ── Lazy-load thumbnails that are currently visible ────────────── */
void thumbs_load_visible(HDC hdc, RECT panelRC) {
    if (!g_app.thumbs) return;
    int panelH = panelRC.bottom - panelRC.top;
    int first  = g_app.thumbScroll / ITEM_H;
    int last   = (g_app.thumbScroll + panelH) / ITEM_H + 1;
    if (first < 0) first = 0;
    if (last >= g_app.thumbCount) last = g_app.thumbCount - 1;

    for (int i = first; i <= last; i++) {
        if (g_app.thumbs[i].hbm) continue;
        g_app.thumbs[i].hbm = make_thumb(hdc,
            g_app.thumbs[i].path,
            &g_app.thumbs[i].w,
            &g_app.thumbs[i].h);
    }
}

/* ── Free all thumbnail bitmaps ─────────────────────────────────── */
void thumbs_free_all(void) {
    if (!g_app.thumbs) return;
    for (int i = 0; i < g_app.thumbCount; i++) {
        if (g_app.thumbs[i].hbm) {
            DeleteObject(g_app.thumbs[i].hbm);
            g_app.thumbs[i].hbm = NULL;
        }
    }
    free(g_app.thumbs);
    g_app.thumbs     = NULL;
    g_app.thumbCount = 0;
    g_app.thumbScroll = 0;
}

/* ── Draw thumbnail panel ───────────────────────────────────────── */
void thumbs_draw(HDC hdc, RECT clientRC) {
    if (!g_app.thumbVisible || g_app.thumbCount == 0) return;

    int panelX = clientRC.right - THUMB_WIDTH;
    int panelT = TB_HEIGHT;
    int panelB = clientRC.bottom;
    int panelH = panelB - panelT;

    /* Background */
    RECT panelRC = { panelX, panelT, clientRC.right, panelB };
    HBRUSH bgBr  = CreateSolidBrush(RGB(26, 26, 33));
    FillRect(hdc, &panelRC, bgBr);
    DeleteObject(bgBr);

    /* Left separator */
    HPEN pen    = CreatePen(PS_SOLID, 1, RGB(55, 55, 72));
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, panelX, panelT, NULL);
    LineTo(hdc,   panelX, panelB);
    SelectObject(hdc, old);
    DeleteObject(pen);

    /* Clip drawing to panel area */
    HRGN clip = CreateRectRgn(panelX, panelT,
                               clientRC.right - SCROLLBAR_W, panelB);
    SelectClipRgn(hdc, clip);

    /* Lazy-load what's on screen */
    RECT pr = { panelX, panelT, clientRC.right, panelB };
    thumbs_load_visible(hdc, pr);

    /* Draw items */
    HDC mdc = CreateCompatibleDC(hdc);
    HFONT font = CreateFontW(11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < g_app.thumbCount; i++) {
        int y = panelT + i * ITEM_H - g_app.thumbScroll;
        if (y + ITEM_H < panelT) continue;
        if (y > panelB)          break;

        BOOL isCurrent = (i == g_app.dirIndex);

        /* Highlight background */
        if (isCurrent) {
            RECT sel = { panelX, y, clientRC.right - SCROLLBAR_W, y + ITEM_H };
            HBRUSH selBr = CreateSolidBrush(RGB(50, 90, 170));
            FillRect(hdc, &sel, selBr);
            DeleteObject(selBr);
        }

        /* Thumbnail bitmap */
        if (g_app.thumbs[i].hbm) {
            BITMAP bm;
            GetObjectW(g_app.thumbs[i].hbm, sizeof(bm), &bm);
            int inner = THUMB_WIDTH - SCROLLBAR_W;
            int tx = panelX + (inner - bm.bmWidth)  / 2;
            int ty = y + (THUMB_H - bm.bmHeight) / 2;
            HGDIOBJ prev = SelectObject(mdc, g_app.thumbs[i].hbm);
            BitBlt(hdc, tx, ty, bm.bmWidth, bm.bmHeight, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, prev);
        }

        /* Filename label (no extension) */
        WCHAR fname[80];
        const WCHAR *slash = wcsrchr(g_app.thumbs[i].path, L'\\');
        wcsncpy_s(fname, 80, slash ? slash + 1 : g_app.thumbs[i].path, 79);
        WCHAR *dot = wcsrchr(fname, L'.');
        if (dot) *dot = 0;

        RECT lblRC = { panelX + 2, y + THUMB_H,
                       clientRC.right - SCROLLBAR_W - 2, y + ITEM_H };
        SetTextColor(hdc, isCurrent ? RGB(255, 255, 255) : RGB(170, 175, 200));
        DrawTextW(hdc, fname, -1, &lblRC,
                  DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_VCENTER);
    }

    DeleteDC(mdc);
    DeleteObject(font);
    SelectClipRgn(hdc, NULL);
    DeleteObject(clip);

    /* ── Scrollbar ──────────────────────────────────────────────── */
    int totalH = g_app.thumbCount * ITEM_H;
    if (totalH > panelH) {
        int sbX   = clientRC.right - SCROLLBAR_W;
        RECT sbBg = { sbX, panelT, clientRC.right, panelB };
        HBRUSH sbBr = CreateSolidBrush(RGB(40, 40, 52));
        FillRect(hdc, &sbBg, sbBr);
        DeleteObject(sbBr);

        /* Thumb bar */
        int barH  = max(20, panelH * panelH / totalH);
        int barY  = panelT + (int)((long long)g_app.thumbScroll *
                                   (panelH - barH) / (totalH - panelH));
        RECT sbBar = { sbX + 1, barY, clientRC.right - 1, barY + barH };
        HBRUSH barBr = CreateSolidBrush(RGB(90, 100, 140));
        FillRect(hdc, &sbBar, barBr);
        DeleteObject(barBr);
    }
}

/* ── Click in thumbnail panel ───────────────────────────────────── */
void thumbs_on_click(HWND hwnd, int x, int y, RECT clientRC) {
    if (!g_app.thumbVisible) return;
    int panelX = clientRC.right - THUMB_WIDTH;
    if (x < panelX) return;

    int relY = y - TB_HEIGHT + g_app.thumbScroll;
    int idx  = relY / ITEM_H;
    if (idx >= 0 && idx < g_app.thumbCount)
        folder_go(hwnd, idx);
}

/* ── Scroll thumbnail panel ─────────────────────────────────────── */
void thumbs_on_scroll(int delta, RECT clientRC) {
    int panelH    = clientRC.bottom - TB_HEIGHT;
    int totalH    = g_app.thumbCount * ITEM_H;
    int maxScroll = totalH - panelH;

    g_app.thumbScroll -= delta / 3;
    if (g_app.thumbScroll < 0)              g_app.thumbScroll = 0;
    if (maxScroll > 0 &&
        g_app.thumbScroll > maxScroll)      g_app.thumbScroll = maxScroll;
}

/* ── Scroll so current image is visible ─────────────────────────── */
void thumbs_scroll_to_current(RECT clientRC) {
    int panelH = clientRC.bottom - TB_HEIGHT;
    int itemY  = g_app.dirIndex * ITEM_H;

    if (itemY < g_app.thumbScroll)
        g_app.thumbScroll = itemY;
    else if (itemY + ITEM_H > g_app.thumbScroll + panelH)
        g_app.thumbScroll = itemY + ITEM_H - panelH;
    if (g_app.thumbScroll < 0) g_app.thumbScroll = 0;
}
