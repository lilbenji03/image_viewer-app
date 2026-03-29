/*
 * main.c  —  WinMain, window procedure, message loop
 */
#include "../include/viewer.h"

/* ── Toolbar hit test ───────────────────────────────────────────── */
typedef struct { int x; int w; int cmd; const WCHAR *label; const WCHAR *tip; } TBBtn;
extern TBBtn s_btns[]; /* defined in draw.c — we replicate for hit-testing */

static int toolbar_hit(int mx, int my) {
    if (my < 0 || my >= TB_HEIGHT) return -1;
    /* Manually list same x/w as draw.c */
    static const struct { int x, w, cmd; } B[] = {
        {   4, 60, IDM_OPEN        },
        {  68, 50, IDM_PREV        },
        { 122, 50, IDM_NEXT        },
        { 178, 46, IDM_FIT         },
        { 228, 46, IDM_RESET       },
        { 278, 50, IDM_ROTATE_CCW  },
        { 332, 50, IDM_ROTATE_CW   },
        { 386, 50, IDM_FLIP_H      },
        { 440, 50, IDM_FLIP_V      },
        { 494, 80, IDM_SLIDESHOW   },
        { 578, 60, IDM_FULLSCREEN  },
        { 642, 60, IDM_INFO        },
        { 706, 60, IDM_THUMB_TOGGLE},
    };
    for (int i = 0; i < (int)(sizeof(B)/sizeof(B[0])); i++)
        if (mx >= B[i].x && mx < B[i].x + B[i].w) return B[i].cmd;
    return -1;
}

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

    /* ── Menu & toolbar commands ── */
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= IDM_RECENT_BASE && id < IDM_RECENT_BASE + MAX_RECENT) {
            int idx = id - IDM_RECENT_BASE;
            if (idx < g_app.recentCount)
                ui_open_path(hwnd, g_app.recent[idx]);
            break;
        }
        switch (id) {
        case IDM_OPEN:         ui_open_file(hwnd);           break;
        case IDM_EXIT:         DestroyWindow(hwnd);          break;
        case IDM_FIT:          ui_fit(hwnd); ui_set_title(hwnd); InvalidateRect(hwnd,NULL,FALSE); break;
        case IDM_RESET:        ui_reset_zoom(hwnd); ui_set_title(hwnd); InvalidateRect(hwnd,NULL,FALSE); break;
        case IDM_FULLSCREEN:   ui_toggle_fullscreen(hwnd);   break;
        case IDM_INFO:         ui_toggle_info(hwnd);         break;
        case IDM_SLIDESHOW:    ui_toggle_slideshow(hwnd);    break;
        case IDM_THUMB_TOGGLE: ui_toggle_thumbs(hwnd);       break;
        case IDM_PREV:         folder_prev(hwnd);             break;
        case IDM_NEXT:         folder_next(hwnd);             break;
        case IDM_ROTATE_CW:
        case IDM_ROTATE_CCW:
        case IDM_FLIP_H:
        case IDM_FLIP_V:       ui_apply_transform(hwnd, id); break;
        case IDM_ABOUT:
            MessageBoxW(hwnd,
                L"Image Viewer v2.0\n\n"
                L"Formats: JPEG, PNG, BMP, GIF, TIFF, TGA, PPM, PGM\n\n"
                L"Shortcuts:\n"
                L"  Ctrl+O       Open file\n"
                L"  ← / →       Previous / Next image\n"
                L"  F            Fit to window\n"
                L"  R or 0       Reset to 100%\n"
                L"  + / -        Zoom in / out\n"
                L"  S            Slideshow\n"
                L"  I            Image info\n"
                L"  T            Thumbnails\n"
                L"  F11          Fullscreen\n"
                L"  Escape       Exit fullscreen / close\n"
                L"  Scroll       Zoom\n"
                L"  Drag         Pan\n"
                L"  Drop file    Open",
                L"About", MB_ICONINFORMATION);
            break;
        }
    } break;

    /* ── Mouse wheel — zoom ── */
    case WM_MOUSEWHEEL: {
        if (!g_app.hBitmap) break;
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);

        /* If mouse is over thumbnail panel, scroll thumbs instead */
        if (g_app.thumbVisible) {
            RECT rc; GetClientRect(hwnd, &rc);
            if (pt.x >= rc.right - THUMB_WIDTH) {
                thumbs_on_scroll(GET_WHEEL_DELTA_WPARAM(wp));
                InvalidateRect(hwnd, NULL, FALSE);
                break;
            }
        }

        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        double factor = delta > 0 ? ZOOM_STEP : 1.0 / ZOOM_STEP;
        ui_zoom_to(hwnd, g_app.zoom * factor, pt.x, pt.y);
    } break;

    /* ── Left button — toolbar hit or start pan ── */
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        int cmd = toolbar_hit(mx, my);
        if (cmd >= 0) {
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
            break;
        }
        /* Thumbnail click? */
        if (g_app.thumbVisible) {
            RECT rc; GetClientRect(hwnd, &rc);
            if (mx >= rc.right - THUMB_WIDTH) {
                thumbs_on_click(hwnd, mx, my, rc);
                break;
            }
        }
        if (!g_app.hBitmap) break;
        SetCapture(hwnd);
        g_app.dragging   = TRUE;
        g_app.dragStart.x = mx; g_app.dragStart.y = my;
        g_app.panXStart  = g_app.panX;
        g_app.panYStart  = g_app.panY;
        SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    } break;

    case WM_MOUSEMOVE:
        if (g_app.dragging) {
            g_app.panX = g_app.panXStart + (GET_X_LPARAM(lp) - g_app.dragStart.x);
            g_app.panY = g_app.panYStart + (GET_Y_LPARAM(lp) - g_app.dragStart.y);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;

    case WM_LBUTTONUP:
        if (g_app.dragging) { ReleaseCapture(); g_app.dragging = FALSE; }
        break;

    /* ── Double-click → fit ── */
    case WM_LBUTTONDBLCLK:
        if (g_app.hBitmap) { ui_fit(hwnd); ui_set_title(hwnd); InvalidateRect(hwnd,NULL,FALSE); }
        break;

    /* ── Keyboard ── */
    case WM_KEYDOWN: {
        RECT rc; GetClientRect(hwnd, &rc);
        int cx = rc.right/2, cy = rc.bottom/2;
        switch (wp) {
        case VK_LEFT:  folder_prev(hwnd); break;
        case VK_RIGHT: folder_next(hwnd); break;
        case 'F': ui_fit(hwnd); ui_set_title(hwnd); InvalidateRect(hwnd,NULL,FALSE); break;
        case 'R': case '0': ui_reset_zoom(hwnd); ui_set_title(hwnd); InvalidateRect(hwnd,NULL,FALSE); break;
        case 'S': ui_toggle_slideshow(hwnd); break;
        case 'I': ui_toggle_info(hwnd); break;
        case 'T': ui_toggle_thumbs(hwnd); break;
        case VK_F11: ui_toggle_fullscreen(hwnd); break;
        case VK_OEM_PLUS: case VK_ADD:
            ui_zoom_to(hwnd, g_app.zoom * ZOOM_STEP, cx, cy); break;
        case VK_OEM_MINUS: case VK_SUBTRACT:
            ui_zoom_to(hwnd, g_app.zoom / ZOOM_STEP, cx, cy); break;
        case VK_ESCAPE:
            if (g_app.fullscreen) ui_toggle_fullscreen(hwnd);
            else DestroyWindow(hwnd);
            break;
        }
    } break;

    /* ── Ctrl+O ── */
    case WM_CHAR:
        if (wp == 15) ui_open_file(hwnd); /* Ctrl+O */
        break;

    /* ── Slideshow timer ── */
    case WM_TIMER:
        if (wp == TIMER_SLIDESHOW && g_app.slideshowOn)
            folder_next(hwnd);
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
        thumbs_free_all();
        if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return 0;
}

/* ── WinMain ────────────────────────────────────────────────────── */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd;
    g_app.hInst = hInst;

    /* Register class */
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"ImgViewerV2";
    RegisterClassExW(&wc);

    /* Build menus */
    HMENU hBar    = CreateMenu();
    HMENU hFile   = CreatePopupMenu();
    HMENU hRecent = CreatePopupMenu();
    HMENU hView   = CreatePopupMenu();
    HMENU hImg    = CreatePopupMenu();
    HMENU hHelp   = CreatePopupMenu();

    AppendMenuW(hFile, MF_STRING,            IDM_OPEN,    L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR,         0, NULL);
    AppendMenuW(hFile, MF_STRING,            IDM_PREV,    L"&Previous\t←");
    AppendMenuW(hFile, MF_STRING,            IDM_NEXT,    L"&Next\t→");
    AppendMenuW(hFile, MF_SEPARATOR,         0, NULL);
    AppendMenuW(hRecent, MF_STRING|MF_GRAYED, IDM_RECENT_BASE, L"(none)");
    AppendMenuW(hFile, MF_POPUP, (UINT_PTR)hRecent, L"Recent &Files");
    AppendMenuW(hFile, MF_SEPARATOR,         0, NULL);
    AppendMenuW(hFile, MF_STRING,            IDM_EXIT,    L"E&xit");

    AppendMenuW(hView, MF_STRING,  IDM_FIT,         L"&Fit to Window\tF");
    AppendMenuW(hView, MF_STRING,  IDM_RESET,       L"&Reset Zoom 100%\tR");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING,  IDM_FULLSCREEN,  L"&Fullscreen\tF11");
    AppendMenuW(hView, MF_STRING,  IDM_INFO,        L"Image &Info\tI");
    AppendMenuW(hView, MF_STRING,  IDM_THUMB_TOGGLE,L"&Thumbnails\tT");
    AppendMenuW(hView, MF_STRING,  IDM_SLIDESHOW,   L"&Slideshow\tS");

    AppendMenuW(hImg, MF_STRING,   IDM_ROTATE_CW,  L"Rotate &Clockwise");
    AppendMenuW(hImg, MF_STRING,   IDM_ROTATE_CCW, L"Rotate &Counter-CW");
    AppendMenuW(hImg, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hImg, MF_STRING,   IDM_FLIP_H,     L"Flip &Horizontal");
    AppendMenuW(hImg, MF_STRING,   IDM_FLIP_V,     L"Flip &Vertical");

    AppendMenuW(hHelp, MF_STRING,  IDM_ABOUT, L"&About");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile,  L"&File");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hView,  L"&View");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hImg,   L"&Image");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp,  L"&Help");

    /* Create window */
    g_app.hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"ImgViewerV2", L"Image Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1100, 750,
        NULL, hBar, hInst, NULL);

    ShowWindow(g_app.hwnd, nShow);
    UpdateWindow(g_app.hwnd);

    /* Open file from command line if given */
    int argc;
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
