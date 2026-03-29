/*
 * ui.c  —  UI actions: open file, zoom, fullscreen, transforms, etc.
 */
#include "../include/viewer.h"
#include "stb_image.h"

/* ── Canvas rect (client minus toolbar and optional thumb panel) ── */
RECT ui_canvas_rect(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    rc.top += TB_HEIGHT;
    if (g_app.thumbVisible) rc.right -= THUMB_WIDTH;
    return rc;
}

/* ── Center image in canvas ─────────────────────────────────────── */
void ui_center_image(HWND hwnd) {
    RECT rc = ui_canvas_rect(hwnd);
    int cw = rc.right  - rc.left;
    int ch = rc.bottom - rc.top;
    g_app.panX = rc.left + (cw - (int)(g_app.imgW * g_app.zoom)) / 2;
    g_app.panY = rc.top  + (ch - (int)(g_app.imgH * g_app.zoom)) / 2;
}

/* ── Fit image to canvas ────────────────────────────────────────── */
void ui_fit(HWND hwnd) {
    if (!g_app.imgW || !g_app.imgH) return;
    RECT rc = ui_canvas_rect(hwnd);
    double zx = (double)(rc.right  - rc.left) / g_app.imgW;
    double zy = (double)(rc.bottom - rc.top)  / g_app.imgH;
    g_app.zoom = g_app.zoomTarget = (zx < zy ? zx : zy);
    if (g_app.zoom > 1.0) g_app.zoom = g_app.zoomTarget = 1.0;
    ui_center_image(hwnd);
}

/* ── Reset zoom to 100% ─────────────────────────────────────────── */
void ui_reset_zoom(HWND hwnd) {
    g_app.zoom = g_app.zoomTarget = 1.0;
    ui_center_image(hwnd);
}

/* ── Zoom toward a screen point (cx, cy) with animation ─────────── */
void ui_zoom_to(HWND hwnd, double nz, int cx, int cy) {
    if (nz < ZOOM_MIN) nz = ZOOM_MIN;
    if (nz > ZOOM_MAX) nz = ZOOM_MAX;
    double old = g_app.zoom;
    g_app.panX = (int)(cx - (cx - g_app.panX) * nz / old);
    g_app.panY = (int)(cy - (cy - g_app.panY) * nz / old);
    g_app.zoom       = nz;
    g_app.zoomTarget = nz;
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ── Update title bar ───────────────────────────────────────────── */
void ui_set_title(HWND hwnd) {
    if (!g_app.imgW) { SetWindowTextW(hwnd, L"Image Viewer"); return; }
    const WCHAR *slash = wcsrchr(g_app.curPath, L'\\');
    WCHAR name[MAX_PATH];
    wcscpy_s(name, MAX_PATH, slash ? slash + 1 : g_app.curPath);
    WCHAR buf[MAX_PATH + 64];
    int idx = g_app.dirCount > 1 ? g_app.dirIndex + 1 : 0;
    if (idx)
        swprintf_s(buf, MAX_PATH+64, L"Image Viewer — %s  [%.0f%%]  (%d/%d)",
                   name, g_app.zoom * 100.0, idx, g_app.dirCount);
    else
        swprintf_s(buf, MAX_PATH+64, L"Image Viewer — %s  [%.0f%%]",
                   name, g_app.zoom * 100.0);
    SetWindowTextW(hwnd, buf);
}

/* ── Open file dialog ───────────────────────────────────────────── */
void ui_open_file(HWND hwnd) {
    WCHAR path[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter =
        L"All Images\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tga;*.ppm;*.pgm;*.tiff;*.tif\0"
        L"JPEG\0*.jpg;*.jpeg\0"
        L"PNG\0*.png\0"
        L"BMP\0*.bmp\0"
        L"GIF\0*.gif\0"
        L"TIFF\0*.tiff;*.tif\0"
        L"All Files\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = L"Open Image";
    if (GetOpenFileNameW(&ofn)) ui_open_path(hwnd, path);
}

/* ── Load and display an image path ─────────────────────────────── */
void ui_open_path(HWND hwnd, const WCHAR *path) {
    Image *img = img_load(path);
    if (!img) return;

    /* Build HBITMAP */
    HDC hdc = GetDC(hwnd);
    HBITMAP hbm = img_to_hbitmap(hdc, img);
    ReleaseDC(hwnd, hdc);
    if (!hbm) { img_free(img); return; }

    /* Store metadata for info panel */
    swprintf_s(g_app.infoText, 512,
        L"File:   %s\n"
        L"Format: %s\n"
        L"Size:   %d × %d px\n"
        L"Depth:  %d-bit",
        PathFindFileNameW(path),
        img->format,
        img->orig_w, img->orig_h,
        img->bit_depth);

    /* Replace bitmap */
    if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
    g_app.hBitmap = hbm;
    g_app.imgW    = img->w;
    g_app.imgH    = img->h;
    wcscpy_s(g_app.curPath, MAX_PATH, path);
    img_free(img);

    /* Scan folder for slideshow / thumbs */
    folder_scan(path);

    /* Refresh thumbnails */
    HDC hdc2 = GetDC(hwnd);
    thumbs_load_all(hdc2);
    ReleaseDC(hwnd, hdc2);

    recent_add(path);

    /* Rebuild recent menu */
    HMENU hBar  = GetMenu(hwnd);
    HMENU hFile = GetSubMenu(hBar, 0);
    HMENU hRecent = GetSubMenu(hFile, 3); /* "Recent Files" is item index 3 */
    if (hRecent) recent_rebuild_menu(hRecent);

    ui_fit(hwnd);
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Toggle fullscreen ──────────────────────────────────────────── */
void ui_toggle_fullscreen(HWND hwnd) {
    if (!g_app.fullscreen) {
        /* Save current state */
        g_app.prevStyle = GetWindowLongW(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &g_app.prevWinRect);

        /* Go borderless over primary monitor */
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right  - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_FRAMECHANGED);
        g_app.fullscreen = TRUE;
    } else {
        SetWindowLongW(hwnd, GWL_STYLE, g_app.prevStyle);
        SetWindowPos(hwnd, NULL,
            g_app.prevWinRect.left, g_app.prevWinRect.top,
            g_app.prevWinRect.right  - g_app.prevWinRect.left,
            g_app.prevWinRect.bottom - g_app.prevWinRect.top,
            SWP_FRAMECHANGED);
        g_app.fullscreen = FALSE;
    }
    ui_fit(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Toggle info panel ──────────────────────────────────────────── */
void ui_toggle_info(HWND hwnd) {
    g_app.infoVisible = !g_app.infoVisible;
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ── Toggle slideshow ───────────────────────────────────────────── */
void ui_toggle_slideshow(HWND hwnd) {
    g_app.slideshowOn = !g_app.slideshowOn;
    if (g_app.slideshowOn)
        SetTimer(hwnd, TIMER_SLIDESHOW, SLIDESHOW_MS, NULL);
    else
        KillTimer(hwnd, TIMER_SLIDESHOW);
    ui_set_title(hwnd);
}

/* ── Toggle thumbnail sidebar ───────────────────────────────────── */
void ui_toggle_thumbs(HWND hwnd) {
    g_app.thumbVisible = !g_app.thumbVisible;
    if (g_app.thumbVisible && g_app.thumbCount == 0 && g_app.dirCount > 0) {
        HDC hdc = GetDC(hwnd);
        thumbs_load_all(hdc);
        ReleaseDC(hwnd, hdc);
    }
    ui_fit(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Apply transform (rotate / flip) ───────────────────────────── */
void ui_apply_transform(HWND hwnd, int kind) {
    if (!g_app.hBitmap || !g_app.imgW) return;

    /* Reconstruct Image from current path */
    Image *img = img_load(g_app.curPath);
    if (!img) return;

    Image *out = NULL;
    switch (kind) {
        case IDM_ROTATE_CW:  out = img_rotate_cw(img);  break;
        case IDM_ROTATE_CCW: out = img_rotate_ccw(img); break;
        case IDM_FLIP_H:     out = img_flip_h(img);     break;
        case IDM_FLIP_V:     out = img_flip_v(img);     break;
    }
    img_free(img);
    if (!out) return;

    HDC hdc = GetDC(hwnd);
    HBITMAP hbm = img_to_hbitmap(hdc, out);
    ReleaseDC(hwnd, hdc);

    if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
    g_app.hBitmap = hbm;
    g_app.imgW    = out->w;
    g_app.imgH    = out->h;
    img_free(out);

    ui_fit(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}
