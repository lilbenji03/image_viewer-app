/*
 * thumbs.c  —  thumbnail sidebar
 */
#include "../include/viewer.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

/* ── Build thumbnail HBITMAP from file ──────────────────────────── */
static HBITMAP make_thumb(HDC hdc, const WCHAR *wpath, int *out_w, int *out_h) {
    char utf8[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, sizeof(utf8), NULL, NULL);

    int w, h, ch;
    uint8_t *data = stbi_load(utf8, &w, &h, &ch, 4);
    if (!data) return NULL;

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;

    /* Compute thumbnail size preserving aspect ratio */
    int tw = THUMB_WIDTH - THUMB_PADDING * 2;
    int th = THUMB_H     - THUMB_PADDING * 2;
    float sx = (float)tw / w, sy = (float)th / h;
    float s  = sx < sy ? sx : sy;
    int dw = (int)(w * s), dh = (int)(h * s);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    /* Resize with stb_image_resize2 */
    uint8_t *resized = (uint8_t *)malloc((size_t)dw * dh * 4);
    stbir_resize_uint8_linear(data, w, h, 0, resized, dw, dh, 0, STBIR_RGBA);
    stbi_image_free(data);

    /* Create DIB */
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth    =  dw;
    bmi.bmiHeader.biHeight   = -dh;
    bmi.bmiHeader.biPlanes   = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hbm && bits) {
        uint8_t *dst = (uint8_t *)bits;
        for (int i = 0; i < dw * dh; i++) {
            dst[i*4+0] = resized[i*4+2]; /* B */
            dst[i*4+1] = resized[i*4+1]; /* G */
            dst[i*4+2] = resized[i*4+0]; /* R */
            dst[i*4+3] = resized[i*4+3]; /* A */
        }
    }
    free(resized);
    return hbm;
}

/* ── Populate thumbs array from dirFiles ────────────────────────── */
void thumbs_load_all(HDC hdc) {
    thumbs_free_all();
    g_app.thumbCount = g_app.dirCount;
    for (int i = 0; i < g_app.dirCount && i < MAX_DIR_FILES; i++) {
        wcscpy_s(g_app.thumbs[i].path, MAX_PATH, g_app.dirFiles[i]);
        g_app.thumbs[i].hbm = NULL; /* lazy load */
    }
    /* Eagerly load first 20 */
    for (int i = 0; i < g_app.thumbCount && i < 20; i++) {
        g_app.thumbs[i].hbm = make_thumb(hdc,
            g_app.thumbs[i].path,
            &g_app.thumbs[i].w,
            &g_app.thumbs[i].h);
    }
}

void thumbs_free_all(void) {
    for (int i = 0; i < g_app.thumbCount; i++) {
        if (g_app.thumbs[i].hbm) {
            DeleteObject(g_app.thumbs[i].hbm);
            g_app.thumbs[i].hbm = NULL;
        }
    }
    g_app.thumbCount = 0;
    g_app.thumbScroll = 0;
}

/* ── Draw thumbnail panel ───────────────────────────────────────── */
void thumbs_draw(HDC hdc, RECT clientRC) {
    if (!g_app.thumbVisible) return;

    int panelX = clientRC.right - THUMB_WIDTH;

    /* Panel background */
    RECT panelRC = { panelX, TB_HEIGHT, clientRC.right, clientRC.bottom };
    HBRUSH bgBr = CreateSolidBrush(RGB(28, 28, 35));
    FillRect(hdc, &panelRC, bgBr);
    DeleteObject(bgBr);

    /* Separator line */
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 80));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, panelX, TB_HEIGHT, NULL);
    LineTo(hdc, panelX, clientRC.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    /* Draw each thumbnail */
    int y = TB_HEIGHT - g_app.thumbScroll;
    HDC mdc = CreateCompatibleDC(hdc);
    HFONT font = CreateFontW(11,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    SelectObject(hdc, font);

    for (int i = 0; i < g_app.thumbCount; i++) {
        int itemH = THUMB_H + 18; /* thumb + filename label */
        if (y + itemH < TB_HEIGHT) { y += itemH; continue; }
        if (y > clientRC.bottom)   break;

        /* Highlight current */
        if (i == g_app.dirIndex) {
            RECT sel = { panelX, y, clientRC.right, y + itemH };
            HBRUSH selBr = CreateSolidBrush(RGB(60, 100, 180));
            FillRect(hdc, &sel, selBr);
            DeleteObject(selBr);
        }

        /* Lazy load thumbnail */
        if (!g_app.thumbs[i].hbm) {
            HDC tmpDC = GetDC(NULL);
            g_app.thumbs[i].hbm = make_thumb(tmpDC,
                g_app.thumbs[i].path,
                &g_app.thumbs[i].w,
                &g_app.thumbs[i].h);
            ReleaseDC(NULL, tmpDC);
        }

        if (g_app.thumbs[i].hbm) {
            BITMAP bm; GetObjectW(g_app.thumbs[i].hbm, sizeof(bm), &bm);
            HGDIOBJ old = SelectObject(mdc, g_app.thumbs[i].hbm);
            int tx = panelX + (THUMB_WIDTH - bm.bmWidth)  / 2;
            int ty = y + THUMB_PADDING;
            BitBlt(hdc, tx, ty, bm.bmWidth, bm.bmHeight, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, old);
        }

        /* Filename label */
        WCHAR fname[64];
        const WCHAR *slash = wcsrchr(g_app.thumbs[i].path, L'\\');
        wcsncpy_s(fname, 64, slash ? slash+1 : g_app.thumbs[i].path, 63);
        /* Truncate extension for display */
        WCHAR *dot = wcsrchr(fname, L'.');
        if (dot) *dot = 0;

        RECT lblRC = { panelX + 2, y + THUMB_H, clientRC.right - 2, y + itemH };
        SetTextColor(hdc, i == g_app.dirIndex ? RGB(255,255,255) : RGB(180,180,200));
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, fname, -1, &lblRC, DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += itemH;
    }

    DeleteDC(mdc);
    DeleteObject(font);
}

void thumbs_on_click(HWND hwnd, int x, int y, RECT clientRC) {
    if (!g_app.thumbVisible) return;
    int panelX = clientRC.right - THUMB_WIDTH;
    if (x < panelX) return;

    int itemH = THUMB_H + 18;
    int relY  = y - TB_HEIGHT + g_app.thumbScroll;
    int idx   = relY / itemH;
    if (idx >= 0 && idx < g_app.thumbCount)
        folder_go(hwnd, idx);
}

void thumbs_on_scroll(int delta) {
    g_app.thumbScroll -= delta / 3;
    int maxScroll = g_app.thumbCount * (THUMB_H + 18) - 200;
    if (g_app.thumbScroll < 0) g_app.thumbScroll = 0;
    if (maxScroll > 0 && g_app.thumbScroll > maxScroll)
        g_app.thumbScroll = maxScroll;
}