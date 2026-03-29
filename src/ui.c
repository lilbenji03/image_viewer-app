/*
 * ui.c  —  UI actions: open, zoom, pan, fullscreen, transforms, etc.
 *
 * Improvements over v2:
 *  - ui_apply_transform() works on in-memory pixels (no disk reload)
 *  - ui_push_undo() / ui_undo_transform() — UNDO_DEPTH-deep stack
 *  - ui_zoom_to() sets zoomTarget; ui_zoom_tick() lerps toward it
 *    (smooth animated zoom via TIMER_ZOOM_ANIM)
 *  - ui_clamp_pan() prevents image from being panned off-screen
 *  - ui_copy_to_clipboard() — copies current bitmap to clipboard
 *  - ui_delete_current() — sends file to Recycle Bin (VK_DELETE)
 *  - ui_save_as() — save a copy via stb_image_write
 *  - ui_set_slideshow_speed() — runtime-configurable interval
 *  - Ctrl+O handled properly in WM_KEYDOWN (no magic 15 constant)
 */
#include "../include/viewer.h"
#include "stb_image.h"

/* ── Canvas rect (client minus toolbar and optional thumb panel) ── */
RECT ui_canvas_rect(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.top += TB_HEIGHT;
    if (g_app.thumbVisible) rc.right -= THUMB_WIDTH;
    return rc;
}

/* ── Center image in the current canvas ─────────────────────────── */
void ui_center_image(HWND hwnd) {
    RECT rc = ui_canvas_rect(hwnd);
    int cw = rc.right  - rc.left;
    int ch = rc.bottom - rc.top;
    g_app.panX = rc.left + (cw - (int)((double)g_app.imgW * g_app.zoom)) / 2;
    g_app.panY = rc.top  + (ch - (int)((double)g_app.imgH * g_app.zoom)) / 2;
}

/* ── Clamp pan so image cannot be dragged fully off-screen ───────── */
void ui_clamp_pan(HWND hwnd) {
    RECT rc = ui_canvas_rect(hwnd);
    int cw = rc.right  - rc.left;
    int ch = rc.bottom - rc.top;
    int iw = (int)((double)g_app.imgW * g_app.zoom);
    int ih = (int)((double)g_app.imgH * g_app.zoom);

    /* Keep at least 40px of the image visible on each axis */
    int margin = 40;
    int minX = rc.left - iw + margin;
    int maxX = rc.right  - margin;
    int minY = rc.top  - ih + margin;
    int maxY = rc.bottom - margin;

    /* If image smaller than canvas, just keep it within canvas */
    if (iw < cw) { minX = rc.left; maxX = rc.right  - iw; }
    if (ih < ch) { minY = rc.top;  maxY = rc.bottom - ih; }

    if (g_app.panX < minX) g_app.panX = minX;
    if (g_app.panX > maxX) g_app.panX = maxX;
    if (g_app.panY < minY) g_app.panY = minY;
    if (g_app.panY > maxY) g_app.panY = maxY;
}

/* ── Fit image to canvas ────────────────────────────────────────── */
void ui_fit(HWND hwnd) {
    if (!g_app.imgW || !g_app.imgH) return;
    RECT rc = ui_canvas_rect(hwnd);
    double zx = (double)(rc.right  - rc.left) / (double)g_app.imgW;
    double zy = (double)(rc.bottom - rc.top)  / (double)g_app.imgH;
    double z  = (zx < zy) ? zx : zy;
    if (z > 1.0) z = 1.0;
    g_app.zoom = g_app.zoomTarget = z;
    ui_center_image(hwnd);
}

/* ── Reset zoom to 100% ─────────────────────────────────────────── */
void ui_reset_zoom(HWND hwnd) {
    g_app.zoom = g_app.zoomTarget = 1.0;
    ui_center_image(hwnd);
}

/* ── Begin zoom toward screen point (cx, cy) ────────────────────── */
void ui_zoom_to(HWND hwnd, double nz, int cx, int cy) {
    if (nz < ZOOM_MIN) nz = ZOOM_MIN;
    if (nz > ZOOM_MAX) nz = ZOOM_MAX;

    /* Anchor the pixel under the cursor */
    double old = g_app.zoom;
    if (old < 1e-9) old = 1e-9;
    g_app.panX = (int)((double)cx - ((double)cx - (double)g_app.panX) * nz / old);
    g_app.panY = (int)((double)cy - ((double)cy - (double)g_app.panY) * nz / old);
    g_app.zoom       = nz;
    g_app.zoomTarget = nz;

    ui_clamp_pan(hwnd);
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);

    /* Start animation timer (fires fast, stops when target reached) */
    SetTimer(hwnd, TIMER_ZOOM_ANIM, ZOOM_ANIM_MS, NULL);
}

/* ── Lerp one tick toward zoomTarget ────────────────────────────── */
void ui_zoom_tick(HWND hwnd) {
    double diff = g_app.zoomTarget - g_app.zoom;
    if (fabs(diff) < 0.001) {
        g_app.zoom = g_app.zoomTarget;
        KillTimer(hwnd, TIMER_ZOOM_ANIM);
    } else {
        int cx = g_app.zoomAnchorX;
        int cy = g_app.zoomAnchorY;
        double nz = g_app.zoom + diff * ZOOM_ANIM_LERP;
        double old = g_app.zoom;
        if (old < 1e-9) old = 1e-9;
        g_app.panX = (int)((double)cx - ((double)cx - (double)g_app.panX) * nz / old);
        g_app.panY = (int)((double)cy - ((double)cy - (double)g_app.panY) * nz / old);
        g_app.zoom = nz;
        ui_clamp_pan(hwnd);
    }
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ── Update title bar ───────────────────────────────────────────── */
void ui_set_title(HWND hwnd) {
    if (!g_app.imgW) {
        SetWindowTextW(hwnd, L"Image Viewer");
        return;
    }
    const WCHAR *slash = wcsrchr(g_app.curPath, L'\\');
    const WCHAR *name  = slash ? slash + 1 : g_app.curPath;

    WCHAR buf[MAX_PATH + 80];
    if (g_app.dirCount > 1) {
        swprintf_s(buf, MAX_PATH + 80,
            L"Image Viewer \u2014 %s  [%.0f%%]  (%d/%d)%s",
            name, g_app.zoom * 100.0,
            g_app.dirIndex + 1, g_app.dirCount,
            g_app.slideshowOn ? L"  \u25B6" : L"");
    } else {
        swprintf_s(buf, MAX_PATH + 80,
            L"Image Viewer \u2014 %s  [%.0f%%]",
            name, g_app.zoom * 100.0);
    }
    SetWindowTextW(hwnd, buf);
}

/* ── Open file dialog ───────────────────────────────────────────── */
void ui_open_file(HWND hwnd) {
    WCHAR path[MAX_PATH] = {0};
    OPENFILENAMEW ofn    = {0};
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
    ofn.lpstrFile  = path;
    ofn.nMaxFile   = MAX_PATH;
    ofn.Flags      = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open Image";
    if (GetOpenFileNameW(&ofn))
        ui_open_path(hwnd, path);
}

/* ── Push current bitmap onto undo stack ────────────────────────── */
void ui_push_undo(void) {
    if (!g_app.hBitmap) return;

    /* If stack is full, drop the oldest entry */
    if (g_app.undoCount == UNDO_DEPTH) {
        if (g_app.undo[0].hbm) DeleteObject(g_app.undo[0].hbm);
        memmove(&g_app.undo[0], &g_app.undo[1],
                (UNDO_DEPTH - 1) * sizeof(UndoEntry));
        g_app.undoCount--;
    }

    /* Duplicate the current HBITMAP */
    HDC hdc  = GetDC(g_app.hwnd);
    HDC mdc  = CreateCompatibleDC(hdc);
    HDC mdc2 = CreateCompatibleDC(hdc);
    BITMAP bm;
    GetObjectW(g_app.hBitmap, sizeof(bm), &bm);
    HBITMAP copy = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
    SelectObject(mdc,  g_app.hBitmap);
    SelectObject(mdc2, copy);
    BitBlt(mdc2, 0, 0, bm.bmWidth, bm.bmHeight, mdc, 0, 0, SRCCOPY);
    DeleteDC(mdc);
    DeleteDC(mdc2);
    ReleaseDC(g_app.hwnd, hdc);

    g_app.undo[g_app.undoCount].hbm = copy;
    g_app.undo[g_app.undoCount].w   = g_app.imgW;
    g_app.undo[g_app.undoCount].h   = g_app.imgH;
    g_app.undoCount++;
}

/* ── Pop undo stack ─────────────────────────────────────────────── */
void ui_undo_transform(HWND hwnd) {
    if (g_app.undoCount == 0) return;
    g_app.undoCount--;
    UndoEntry *e = &g_app.undo[g_app.undoCount];

    if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
    g_app.hBitmap = e->hbm;
    g_app.imgW    = e->w;
    g_app.imgH    = e->h;
    e->hbm = NULL;

    ui_fit(hwnd);
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Load and display an image path ─────────────────────────────── */
void ui_open_path(HWND hwnd, const WCHAR *path) {
    Image *img = img_load(path);
    if (!img) return;

    HDC hdc = GetDC(hwnd);
    HBITMAP hbm = img_to_hbitmap(hdc, img);
    ReleaseDC(hwnd, hdc);
    if (!hbm) { img_free(img); return; }

    /* Build info panel text */
    swprintf_s(g_app.infoText, INFO_TEXT_LEN,
        L"File:   %s\n"
        L"Format: %s\n"
        L"Size:   %d \u00D7 %d px\n"
        L"Depth:  %d-bit",
        PathFindFileNameW(path),
        img->format,
        img->orig_w, img->orig_h,
        img->bit_depth);

    /* Clear undo stack when opening a new file */
    for (int i = 0; i < g_app.undoCount; i++) {
        if (g_app.undo[i].hbm) DeleteObject(g_app.undo[i].hbm);
        g_app.undo[i].hbm = NULL;
    }
    g_app.undoCount = 0;

    if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
    g_app.hBitmap = hbm;
    g_app.imgW    = img->w;
    g_app.imgH    = img->h;
    wcscpy_s(g_app.curPath, MAX_PATH, path);
    img_free(img);

    /* Scan folder (re-builds dirFiles) */
    folder_scan(path);

    /* Sync + lazy-load thumbnails */
    thumbs_sync();
    if (g_app.thumbVisible) {
        HDC hdc2 = GetDC(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        RECT pr = { rc.right - THUMB_WIDTH, TB_HEIGHT,
                    rc.right, rc.bottom };
        thumbs_load_visible(hdc2, pr);
        ReleaseDC(hwnd, hdc2);
    }

    recent_add(path);
    HMENU hBar   = GetMenu(hwnd);
    HMENU hFile  = GetSubMenu(hBar, 0);
    HMENU hRecent = GetSubMenu(hFile, 3);
    if (hRecent) recent_rebuild_menu(hRecent);

    ui_fit(hwnd);
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Save a copy of the current image ───────────────────────────── */
void ui_save_as(HWND hwnd) {
    if (!g_app.hBitmap) return;

    WCHAR path[MAX_PATH] = {0};
    /* Pre-fill with current filename */
    const WCHAR *cur = wcsrchr(g_app.curPath, L'\\');
    if (cur) wcscpy_s(path, MAX_PATH, cur + 1);

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter =
        L"PNG\0*.png\0"
        L"JPEG\0*.jpg\0"
        L"BMP\0*.bmp\0";
    ofn.lpstrFile        = path;
    ofn.nMaxFile         = MAX_PATH;
    ofn.lpstrDefExt      = L"png";
    ofn.lpstrTitle       = L"Save Image As";
    ofn.Flags            = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return;

    /* Re-load from source to get pixel data */
    Image *img = img_load(g_app.curPath);
    if (!img) return;
    img_save(img, path);
    img_free(img);
}

/* ── Copy current image to clipboard ────────────────────────────── */
void ui_copy_to_clipboard(HWND hwnd) {
    if (!g_app.hBitmap) return;

    /* Duplicate the bitmap (clipboard takes ownership) */
    HDC hdc  = GetDC(hwnd);
    HDC mdc  = CreateCompatibleDC(hdc);
    HDC mdc2 = CreateCompatibleDC(hdc);
    BITMAP bm;
    GetObjectW(g_app.hBitmap, sizeof(bm), &bm);
    HBITMAP copy = CreateCompatibleBitmap(hdc, bm.bmWidth, bm.bmHeight);
    SelectObject(mdc,  g_app.hBitmap);
    SelectObject(mdc2, copy);
    BitBlt(mdc2, 0, 0, bm.bmWidth, bm.bmHeight, mdc, 0, 0, SRCCOPY);
    DeleteDC(mdc);
    DeleteDC(mdc2);
    ReleaseDC(hwnd, hdc);

    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, copy);
        CloseClipboard();
    } else {
        DeleteObject(copy);
    }
}

/* ── Delete current file (Recycle Bin) ──────────────────────────── */
void ui_delete_current(HWND hwnd) {
    if (!g_app.hBitmap || !g_app.curPath[0]) return;

    int res = MessageBoxW(hwnd,
        L"Move this image to the Recycle Bin?",
        L"Delete Image",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    if (res != IDYES) return;

    /* Remember next image before deleting */
    int nextIdx = g_app.dirIndex;

    /* SHFileOperation requires double-null-terminated string */
    WCHAR buf[MAX_PATH + 2];
    wcscpy_s(buf, MAX_PATH, g_app.curPath);
    buf[wcslen(buf) + 1] = L'\0';

    SHFILEOPSTRUCTW op = {0};
    op.hwnd   = hwnd;
    op.wFunc  = FO_DELETE;
    op.pFrom  = buf;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    SHFileOperationW(&op);

    /* Free current bitmap */
    if (g_app.hBitmap) { DeleteObject(g_app.hBitmap); g_app.hBitmap = NULL; }
    g_app.imgW = g_app.imgH = 0;

    /* Remove from dir list */
    if (g_app.dirFiles) {
        free(g_app.dirFiles[nextIdx]);
        for (int i = nextIdx; i < g_app.dirCount - 1; i++)
            g_app.dirFiles[i] = g_app.dirFiles[i + 1];
        g_app.dirCount--;
    }
    thumbs_free_all();

    if (g_app.dirCount == 0) {
        g_app.dirIndex = 0;
        g_app.curPath[0] = 0;
        ui_set_title(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return;
    }

    if (nextIdx >= g_app.dirCount) nextIdx = g_app.dirCount - 1;
    g_app.dirIndex = nextIdx - 1; /* folder_go will increment */
    folder_go(hwnd, nextIdx);
}

/* ── Toggle fullscreen ──────────────────────────────────────────── */
void ui_toggle_fullscreen(HWND hwnd) {
    if (!g_app.fullscreen) {
        g_app.prevStyle = GetWindowLongW(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &g_app.prevWinRect);

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
            g_app.prevWinRect.left,
            g_app.prevWinRect.top,
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
        SetTimer(hwnd, TIMER_SLIDESHOW, (UINT)g_app.slideshowSpeed, NULL);
    else
        KillTimer(hwnd, TIMER_SLIDESHOW);
    ui_set_title(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ── Set slideshow speed ────────────────────────────────────────── */
void ui_set_slideshow_speed(HWND hwnd, SlideshowSpeed spd) {
    g_app.slideshowSpeed = spd;
    if (g_app.slideshowOn) {
        KillTimer(hwnd, TIMER_SLIDESHOW);
        SetTimer(hwnd, TIMER_SLIDESHOW, (UINT)spd, NULL);
    }
}

/* ── Toggle thumbnail sidebar ───────────────────────────────────── */
void ui_toggle_thumbs(HWND hwnd) {
    g_app.thumbVisible = !g_app.thumbVisible;
    if (g_app.thumbVisible) {
        thumbs_sync();
        HDC hdc = GetDC(hwnd);
        RECT rc; GetClientRect(hwnd, &rc);
        RECT pr = { rc.right - THUMB_WIDTH, TB_HEIGHT, rc.right, rc.bottom };
        thumbs_load_visible(hdc, pr);
        ReleaseDC(hwnd, hdc);
    }
    ui_fit(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
}

/* ── Apply transform (rotate / flip) on in-memory pixels ─────────
 *
 *  FIX: Previously reloaded from disk on every call, meaning
 *  transforms didn't stack (e.g. two CW rotations ≠ 180°).
 *  Now we:
 *    1. Push current bitmap to undo stack
 *    2. Extract pixels from HBITMAP into an Image
 *    3. Apply the transform in memory
 *    4. Convert back to HBITMAP
 *  Transforms now stack correctly.
 * ───────────────────────────────────────────────────────────────── */
void ui_apply_transform(HWND hwnd, int kind) {
    if (!g_app.hBitmap || !g_app.imgW) return;

    /* Push undo before modifying */
    ui_push_undo();

    /* Extract pixels from current HBITMAP into an Image */
    Image *src = (Image *)calloc(1, sizeof(Image));
    if (!src) return;
    src->w = src->orig_w = g_app.imgW;
    src->h = src->orig_h = g_app.imgH;
    src->px = (Pixel *)malloc((size_t)src->w * src->h * sizeof(Pixel));
    if (!src->px) { free(src); return; }

    /* Read pixels back from HBITMAP (BGRA → RGBA) */
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       =  src->w;
    bmi.bmiHeader.biHeight      = -src->h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    uint8_t *bgra = (uint8_t *)malloc((size_t)src->w * src->h * 4);
    if (!bgra) { img_free(src); return; }

    HDC hdc = GetDC(hwnd);
    GetDIBits(hdc, g_app.hBitmap, 0, (UINT)src->h, bgra, &bmi, DIB_RGB_COLORS);
    ReleaseDC(hwnd, hdc);

    for (int i = 0; i < src->w * src->h; i++) {
        src->px[i].b = bgra[i*4+0];
        src->px[i].g = bgra[i*4+1];
        src->px[i].r = bgra[i*4+2];
        src->px[i].a = bgra[i*4+3];
    }
    free(bgra);

    /* Copy path/format from app state */
    wcscpy_s(src->path,   MAX_PATH, g_app.curPath);
    wcscpy_s(src->format, 32,       L"mem");

    /* Apply transform */
    Image *out = NULL;
    switch (kind) {
        case IDM_ROTATE_CW:  out = img_rotate_cw(src);  break;
        case IDM_ROTATE_CCW: out = img_rotate_ccw(src); break;
        case IDM_FLIP_H:     out = img_flip_h(src);     break;
        case IDM_FLIP_V:     out = img_flip_v(src);     break;
    }
    img_free(src);
    if (!out) return;

    hdc = GetDC(hwnd);
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
