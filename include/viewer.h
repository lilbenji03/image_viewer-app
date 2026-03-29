#pragma once
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Menu IDs ─────────────────────────────────────────────────────── */
#define IDM_OPEN          101
#define IDM_EXIT          102
#define IDM_FIT           103
#define IDM_RESET         104
#define IDM_ABOUT         105
#define IDM_FULLSCREEN    106
#define IDM_ROTATE_CW     107
#define IDM_ROTATE_CCW    108
#define IDM_FLIP_H        109
#define IDM_FLIP_V        110
#define IDM_INFO          111
#define IDM_SLIDESHOW     112
#define IDM_PREV          113
#define IDM_NEXT          114
#define IDM_RECENT_BASE   200   /* 200–209 = recent files */
#define IDM_THUMB_TOGGLE  210

/* ── Timer IDs ────────────────────────────────────────────────────── */
#define TIMER_SLIDESHOW   1
#define TIMER_ZOOM_ANIM   2
#define SLIDESHOW_MS      3000

/* ── Zoom limits ──────────────────────────────────────────────────── */
#define ZOOM_MIN   0.02
#define ZOOM_MAX   64.0
#define ZOOM_STEP  1.15

/* ── Toolbar / thumbnail metrics ─────────────────────────────────── */
#define TB_HEIGHT      40
#define THUMB_WIDTH   110
#define THUMB_H        90
#define THUMB_PADDING   8
#define MAX_RECENT     10
#define MAX_DIR_FILES 512

/* ── Pixel & Image ────────────────────────────────────────────────── */
typedef struct { uint8_t r, g, b, a; } Pixel;

typedef struct {
    int      w, h;
    Pixel   *px;          /* RGBA row-major */
    int      rotation;    /* 0,90,180,270   */
    BOOL     flip_h;
    BOOL     flip_v;
    WCHAR    path[MAX_PATH];
    WCHAR    format[16];
    int      orig_w, orig_h;
    int      bit_depth;
} Image;

/* ── Thumbnail entry ──────────────────────────────────────────────── */
typedef struct {
    WCHAR    path[MAX_PATH];
    HBITMAP  hbm;         /* NULL = not yet loaded */
    int      w, h;        /* original dimensions   */
} Thumb;

/* ── App state (single global struct) ────────────────────────────── */
typedef struct {
    /* current image */
    HBITMAP  hBitmap;
    int      imgW, imgH;
    WCHAR    curPath[MAX_PATH];

    /* zoom / pan */
    double   zoom;
    double   zoomTarget;
    int      panX, panY;
    BOOL     dragging;
    POINT    dragStart;
    int      panXStart, panYStart;

    /* folder / slideshow */
    WCHAR    dirFiles[MAX_DIR_FILES][MAX_PATH];
    int      dirCount;
    int      dirIndex;
    BOOL     slideshowOn;

    /* recent files */
    WCHAR    recent[MAX_RECENT][MAX_PATH];
    int      recentCount;

    /* thumbnails */
    Thumb    thumbs[MAX_DIR_FILES];
    int      thumbCount;
    BOOL     thumbVisible;
    int      thumbScroll;   /* pixel scroll offset */

    /* UI */
    BOOL     fullscreen;
    RECT     prevWinRect;   /* saved rect before fullscreen */
    DWORD    prevStyle;
    BOOL     infoVisible;

    /* image metadata */
    WCHAR    infoText[512];

    HWND     hwnd;
    HINSTANCE hInst;
} AppState;

extern AppState g_app;

/* ── Function declarations (implemented across .c files) ─────────── */

/* image_io.c */
Image  *img_load(const WCHAR *path);
void    img_free(Image *img);
HBITMAP img_to_hbitmap(HDC hdc, const Image *img);

/* transform.c */
Image  *img_rotate_cw(Image *src);
Image  *img_rotate_ccw(Image *src);
Image  *img_flip_h(Image *src);
Image  *img_flip_v(Image *src);

/* folder.c */
void    folder_scan(const WCHAR *path);
void    folder_go(HWND hwnd, int index);
void    folder_next(HWND hwnd);
void    folder_prev(HWND hwnd);

/* thumbs.c */
void    thumbs_load_all(HDC hdc);
void    thumbs_free_all(void);
void    thumbs_draw(HDC hdc, RECT clientRC);
void    thumbs_on_click(HWND hwnd, int x, int y, RECT clientRC);
void    thumbs_on_scroll(int delta);

/* recent.c */
void    recent_add(const WCHAR *path);
void    recent_rebuild_menu(HMENU hMenu);

/* ui.c  */
void    ui_open_file(HWND hwnd);
void    ui_open_path(HWND hwnd, const WCHAR *path);
void    ui_set_title(HWND hwnd);
void    ui_fit(HWND hwnd);
void    ui_reset_zoom(HWND hwnd);
void    ui_zoom_to(HWND hwnd, double nz, int cx, int cy);
void    ui_toggle_fullscreen(HWND hwnd);
void    ui_toggle_info(HWND hwnd);
void    ui_toggle_slideshow(HWND hwnd);
void    ui_toggle_thumbs(HWND hwnd);
void    ui_apply_transform(HWND hwnd, int kind);
void    ui_center_image(HWND hwnd);
RECT    ui_canvas_rect(HWND hwnd);   /* client area minus toolbar/thumbs */

/* draw.c */
void    draw_toolbar(HDC hdc, HWND hwnd);
void    draw_image(HDC hdc, HWND hwnd);
void    draw_info(HDC hdc, HWND hwnd);
void    draw_scene(HWND hwnd);
