/*
 * main.c  —  WinMain, window procedure, message loop
 *
 * Improvements over v2:
 *  - toolbar_hit() removed — now lives in draw.c and uses shared g_btns[]
 *  - Ctrl+O handled via GetKeyState() in WM_KEYDOWN (no magic constant 15)
 *  - Ctrl+C → copy to clipboard
 *  - Ctrl+Z → undo transform
 *  - Ctrl+S → save as
 *  - Delete  → move to Recycle Bin
 *  - Slideshow speed submenu (1s / 3s / 5s / 10s)
 *  - WM_GETMINMAXINFO: enforces a minimum window size
 *  - WM_DESTROY: folder_free() called to release dynamic dir list
 *  - Default slideshow speed initialised to 3s in WinMain
 */
#include "../include/viewer.h"

/* ── Window procedure ───────────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    /* ── Paint (double-buffered) ── */
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        draw_scene(hwnd);
        EndPaint(hwnd, &ps);
    } return 0;
    case WM_ERASEBKGND: return 1;

    /* ── Resize ── */
    case WM_SIZE:
        if (g_app.hBitmap) ui_fit(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    /* ── Minimum window size ── */
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mmi = (MINMAXINFO *)lp;
        mmi->ptMinTrackSize.x = 500;
        mmi->ptMinTrackSize.y = 350;
    } return 0;

    /* ── Menu & toolbar commands ── */
    case WM_COMMAND: {
        int id = LOWORD(wp);

        /* Recent files */
        if (id >= IDM_RECENT_BASE && id < IDM_RECENT_BASE + MAX_RECENT) {
            int idx = id - IDM_RECENT_BASE;
            if (idx < g_app.recentCount)
                ui_open_path(hwnd, g_app.recent[idx]);
            break;
        }

        switch (id) {
        case IDM_OPEN:          ui_open_file(hwnd);                  break;
        case IDM_SAVE_AS:       ui_save_as(hwnd);                    break;
        case IDM_COPY:          ui_copy_to_clipboard(hwnd);          break;
        case IDM_DELETE:        ui_delete_current(hwnd);             break;
        case IDM_EXIT:          DestroyWindow(hwnd);                 break;

        case IDM_FIT:
            ui_fit(hwnd);
            ui_set_title(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_RESET:
            ui_reset_zoom(hwnd);
            ui_set_title(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case IDM_ZOOM_IN: {
            RECT rc; GetClientRect(hwnd, &rc);
            ui_zoom_to(hwnd, g_app.zoom * ZOOM_STEP, rc.right/2, rc.bottom/2);
        } break;
        case IDM_ZOOM_OUT: {
            RECT rc; GetClientRect(hwnd, &rc);
            ui_zoom_to(hwnd, g_app.zoom / ZOOM_STEP, rc.right/2, rc.bottom/2);
        } break;

        case IDM_FULLSCREEN:    ui_toggle_fullscreen(hwnd);          break;
        case IDM_INFO:          ui_toggle_info(hwnd);                break;
        case IDM_SLIDESHOW:     ui_toggle_slideshow(hwnd);           break;
        case IDM_THUMB_TOGGLE:  ui_toggle_thumbs(hwnd);              break;
        case IDM_PREV:          folder_prev(hwnd);                   break;
        case IDM_NEXT:          folder_next(hwnd);                   break;
        case IDM_UNDO:          ui_undo_transform(hwnd);             break;

        case IDM_ROTATE_CW:
        case IDM_ROTATE_CCW:
        case IDM_FLIP_H:
        case IDM_FLIP_V:        ui_apply_transform(hwnd, id);        break;

        case IDM_SLIDESHOW_1S:  ui_set_slideshow_speed(hwnd, SS_1S);  break;
        case IDM_SLIDESHOW_3S:  ui_set_slideshow_speed(hwnd, SS_3S);  break;
        case IDM_SLIDESHOW_5S:  ui_set_slideshow_speed(hwnd, SS_5S);  break;
        case IDM_SLIDESHOW_10S: ui_set_slideshow_speed(hwnd, SS_10S); break;

        case IDM_ABOUT:
            MessageBoxW(hwnd,
                L"Image Viewer v3.0\n\n"
                L"Formats: JPEG, PNG, BMP, GIF, TIFF, TGA, PPM, PGM\n\n"
                L"Keyboard shortcuts:\n"
                L"  Ctrl+O      Open file\n"
                L"  Ctrl+S      Save a copy\n"
                L"  Ctrl+C      Copy to clipboard\n"
                L"  Ctrl+Z      Undo transform\n"
                L"  Delete      Move to Recycle Bin\n"
                L"  \u2190 / \u2192     Previous / Next image\n"
                L"  F           Fit to window\n"
                L"  R or 0      Reset to 100%\n"
                L"  + / -       Zoom in / out\n"
                L"  S           Slideshow\n"
                L"  I           Image info\n"
                L"  T           Thumbnails\n"
                L"  F11         Fullscreen\n"
                L"  Escape      Exit fullscreen / close\n"
                L"  Scroll      Zoom\n"
                L"  Drag        Pan",
                L"About", MB_ICONINFORMATION);
            break;
        }
    } break;

    /* ── Mouse wheel ── */
    case WM_MOUSEWHEEL: {
        if (!g_app.hBitmap) break;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);

        /* If over thumbnail panel, scroll thumbs */
        if (g_app.thumbVisible) {
            RECT rc; GetClientRect(hwnd, &rc);
            if (pt.x >= rc.right - THUMB_WIDTH) {
                thumbs_on_scroll(GET_WHEEL_DELTA_WPARAM(wp), rc);
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            }
        }

        int    delta  = GET_WHEEL_DELTA_WPARAM(wp);
        double factor = delta > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP;
        /* Store anchor for animation */
        g_app.zoomAnchorX = pt.x;
        g_app.zoomAnchorY = pt.y;
        ui_zoom_to(hwnd, g_app.zoom * factor, pt.x, pt.y);
    } break;

    /* ── Left button down — toolbar or pan start ── */
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);

        /* Toolbar hit */
        int cmd = toolbar_hit(mx, my);
        if (cmd >= 0) {
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
            break;
        }

        /* Thumbnail click */
        if (g_app.thumbVisible) {
            RECT rc; GetClientRect(hwnd, &rc);
            if (mx >= rc.right - THUMB_WIDTH) {
                thumbs_on_click(hwnd, mx, my, rc);
                break;
            }
        }

        if (!g_app.hBitmap) break;
        SetCapture(hwnd);
        g_app.dragging    = TRUE;
        g_app.dragStart.x = mx;
        g_app.dragStart.y = my;
        g_app.panXStart   = g_app.panX;
        g_app.panYStart   = g_app.panY;
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    } break;

    case WM_MOUSEMOVE:
        if (g_app.dragging) {
            g_app.panX = g_app.panXStart + (GET_X_LPARAM(lp) - g_app.dragStart.x);
            g_app.panY = g_app.panYStart + (GET_Y_LPARAM(lp) - g_app.dragStart.y);
            ui_clamp_pan(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONUP:
        if (g_app.dragging) { ReleaseCapture(); g_app.dragging = FALSE; }
        break;

    /* ── Double-click → fit ── */
    case WM_LBUTTONDBLCLK:
        if (g_app.hBitmap) {
            ui_fit(hwnd);
            ui_set_title(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    /* ── Keyboard ── */
    case WM_KEYDOWN: {
        BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        RECT rc;   GetClientRect(hwnd, &rc);
        int  cx = rc.right / 2, cy = rc.bottom / 2;

        if (ctrl) {
            switch (wp) {
            case 'O': ui_open_file(hwnd);          break;
            case 'S': ui_save_as(hwnd);            break;
            case 'C': ui_copy_to_clipboard(hwnd);  break;
            case 'Z': ui_undo_transform(hwnd);     break;
            }
            break;  /* don't fall through to non-ctrl keys */
        }

        switch (wp) {
        case VK_LEFT:  folder_prev(hwnd); break;
        case VK_RIGHT: folder_next(hwnd); break;
        case 'F':
            ui_fit(hwnd);
            ui_set_title(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case 'R': case '0':
            ui_reset_zoom(hwnd);
            ui_set_title(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case 'S': ui_toggle_slideshow(hwnd);  break;
        case 'I': ui_toggle_info(hwnd);       break;
        case 'T': ui_toggle_thumbs(hwnd);     break;
        case VK_F11: ui_toggle_fullscreen(hwnd); break;
        case VK_DELETE: ui_delete_current(hwnd); break;
        case VK_OEM_PLUS:  case VK_ADD:
            g_app.zoomAnchorX = cx; g_app.zoomAnchorY = cy;
            ui_zoom_to(hwnd, g_app.zoom * ZOOM_STEP, cx, cy);
            break;
        case VK_OEM_MINUS: case VK_SUBTRACT:
            g_app.zoomAnchorX = cx; g_app.zoomAnchorY = cy;
            ui_zoom_to(hwnd, g_app.zoom / ZOOM_STEP, cx, cy);
            break;
        case VK_ESCAPE:
            if (g_app.fullscreen) ui_toggle_fullscreen(hwnd);
            else DestroyWindow(hwnd);
            break;
        }
    } break;

    /* ── Timers ── */
    case WM_TIMER:
        if (wp == TIMER_SLIDESHOW && g_app.slideshowOn)
            folder_next(hwnd);
        else if (wp == TIMER_ZOOM_ANIM)
            ui_zoom_tick(hwnd);
        break;

    /* ── Drag and drop ── */
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wp;
        WCHAR path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH))
            ui_open_path(hwnd, path);
        DragFinish(drop);
    } break;

    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SLIDESHOW);
        KillTimer(hwnd, TIMER_ZOOM_ANIM);
        thumbs_free_all();
        folder_free();
        if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
        /* Free undo stack */
        for (int i = 0; i < g_app.undoCount; i++)
            if (g_app.undo[i].hbm) DeleteObject(g_app.undo[i].hbm);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

/* ── WinMain ────────────────────────────────────────────────────── */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev,
                   LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;
    g_app.hInst          = hInst;
    g_app.slideshowSpeed = SS_3S;   /* default slideshow interval */

    /* Register window class */
    WNDCLASSEXW wc  = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ImgViewerV3";
    RegisterClassExW(&wc);

    /* ── Build menus ── */
    HMENU hBar     = CreateMenu();
    HMENU hFile    = CreatePopupMenu();
    HMENU hRecent  = CreatePopupMenu();
    HMENU hView    = CreatePopupMenu();
    HMENU hImg     = CreatePopupMenu();
    HMENU hSlide   = CreatePopupMenu();  /* slideshow speed submenu */
    HMENU hHelp    = CreatePopupMenu();

    /* File menu */
    AppendMenuW(hFile, MF_STRING,              IDM_OPEN,    L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,              IDM_SAVE_AS, L"&Save a Copy...\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,              IDM_COPY,    L"&Copy to Clipboard\tCtrl+C");
    AppendMenuW(hFile, MF_SEPARATOR,           0, NULL);
    AppendMenuW(hFile, MF_STRING,              IDM_PREV,    L"&Previous\t\u2190");
    AppendMenuW(hFile, MF_STRING,              IDM_NEXT,    L"&Next\t\u2192");
    AppendMenuW(hFile, MF_SEPARATOR,           0, NULL);
    AppendMenuW(hRecent, MF_STRING|MF_GRAYED,  IDM_RECENT_BASE, L"(none)");
    AppendMenuW(hFile, MF_POPUP, (UINT_PTR)hRecent, L"Recent &Files");
    AppendMenuW(hFile, MF_SEPARATOR,           0, NULL);
    AppendMenuW(hFile, MF_STRING,              IDM_DELETE,  L"Move to &Recycle Bin\tDel");
    AppendMenuW(hFile, MF_SEPARATOR,           0, NULL);
    AppendMenuW(hFile, MF_STRING,              IDM_EXIT,    L"E&xit");

    /* View menu */
    AppendMenuW(hView, MF_STRING,  IDM_FIT,          L"&Fit to Window\tF");
    AppendMenuW(hView, MF_STRING,  IDM_RESET,        L"&Reset Zoom 100%\tR");
    AppendMenuW(hView, MF_STRING,  IDM_ZOOM_IN,      L"Zoom &In\t+");
    AppendMenuW(hView, MF_STRING,  IDM_ZOOM_OUT,     L"Zoom &Out\t-");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING,  IDM_FULLSCREEN,   L"&Fullscreen\tF11");
    AppendMenuW(hView, MF_STRING,  IDM_INFO,         L"Image &Info\tI");
    AppendMenuW(hView, MF_STRING,  IDM_THUMB_TOGGLE, L"&Thumbnails\tT");

    /* Slideshow speed submenu */
    AppendMenuW(hSlide, MF_STRING, IDM_SLIDESHOW_1S,  L"&1 second");
    AppendMenuW(hSlide, MF_STRING, IDM_SLIDESHOW_3S,  L"&3 seconds (default)");
    AppendMenuW(hSlide, MF_STRING, IDM_SLIDESHOW_5S,  L"&5 seconds");
    AppendMenuW(hSlide, MF_STRING, IDM_SLIDESHOW_10S, L"1&0 seconds");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING,  IDM_SLIDESHOW,    L"&Slideshow\tS");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hSlide,   L"Slideshow S&peed");

    /* Image menu */
    AppendMenuW(hImg, MF_STRING,   IDM_ROTATE_CW,   L"Rotate &Clockwise");
    AppendMenuW(hImg, MF_STRING,   IDM_ROTATE_CCW,  L"Rotate &Counter-CW");
    AppendMenuW(hImg, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hImg, MF_STRING,   IDM_FLIP_H,      L"Flip &Horizontal");
    AppendMenuW(hImg, MF_STRING,   IDM_FLIP_V,      L"Flip &Vertical");
    AppendMenuW(hImg, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hImg, MF_STRING,   IDM_UNDO,        L"&Undo Transform\tCtrl+Z");

    /* Help */
    AppendMenuW(hHelp, MF_STRING,  IDM_ABOUT, L"&About");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile,  L"&File");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView,  L"&View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hImg,   L"&Image");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp,  L"&Help");

    /* Create window */
    g_app.hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"ImgViewerV3", L"Image Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1150, 780,
        NULL, hBar, hInst, NULL);

    ShowWindow(g_app.hwnd, nShow);
    UpdateWindow(g_app.hwnd);

    /* Open file from command line */
    int    argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc >= 2) ui_open_path(g_app.hwnd, argv[1]);
    LocalFree(argv);

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
