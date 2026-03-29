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

/* ── Menu / command IDs ───────────────────────────────────────────── */
#define IDM_OPEN            101
#define IDM_EXIT            102
#define IDM_FIT             103
#define IDM_RESET           104
#define IDM_ABOUT           105
#define IDM_FULLSCREEN      106
#define IDM_ROTATE_CW       107
#define IDM_ROTATE_CCW      108
#define IDM_FLIP_H          109
#define IDM_FLIP_V          110
#define IDM_INFO            111
#define IDM_SLIDESHOW       112
#define IDM_PREV            113
#define IDM_NEXT            114
#define IDM_COPY            115
#define IDM_DELETE          116
#define IDM_SAVE_AS         117
#define IDM_ZOOM_IN         118
#define IDM_ZOOM_OUT        119
#define IDM_SLIDESHOW_1S    120
#define IDM_SLIDESHOW_3S    121
#define IDM_SLIDESHOW_5S    122
#define IDM_SLIDESHOW_10S   123
#define IDM_UNDO            124

#define IDM_RECENT_BASE     200   /* 200–219 = recent files            */
#define IDM_THUMB_TOGGLE    220

/* ── Timer IDs ────────────────────────────────────────────────────── */
#define TIMER_SLIDESHOW     1
#define TIMER_ZOOM_ANIM     2

/* ── Zoom ─────────────────────────────────────────────────────────── */
#define ZOOM_MIN            0.02
#define ZOOM_MAX            64.0
#define ZOOM_STEP           1.15
#define ZOOM_ANIM_MS        12     /* timer interval for smooth zoom   */
#define ZOOM_ANIM_LERP      0.25   /* lerp factor per tick             */

/* ── Layout ───────────────────────────────────────────────────────── */
#define TB_HEIGHT           40
#define THUMB_WIDTH         120
#define THUMB_H             90
#define THUMB_PADDING       8
#define SCROLLBAR_W         5      /* width of thumb-panel scrollbar   */

/* ── Limits ───────────────────────────────────────────────────────── */
#define MAX_RECENT          20
#define MAX_DIR_FILES       4096   /* was 512, now dynamic via realloc */
#define INFO_TEXT_LEN       1024   /* was 512                          */
#define UNDO_DEPTH          10     /* transform undo stack size        */

/* ── Pixel & Image ────────────────────────────────────────────────── */
typedef struct { uint8_t r, g, b, a; } Pixel;

typedef struct {
    int      w, h;
    Pixel   *px;           /* RGBA row-major                          */
    int      rotation;     /* 0, 90, 180, 270                         */
    BOOL     flip_h;
    BOOL     flip_v;
    WCHAR    path[MAX_PATH];
    WCHAR    format[32];
    int      orig_w, orig_h;
    int      bit_depth;
} Image;

/* ── Thumbnail entry ──────────────────────────────────────────────── */
typedef struct {
    WCHAR    path[MAX_PATH];
    HBITMAP  hbm;           /* NULL = not yet loaded                  */
    int      w, h;
} Thumb;

/* ── Slideshow speed options ──────────────────────────────────────── */
typedef enum {
    SS_1S  = 1000,
    SS_3S  = 3000,
    SS_5S  = 5000,
    SS_10S = 10000
} SlideshowSpeed;

/* ── Transform undo stack entry ───────────────────────────────────── */
typedef struct {
    HBITMAP hbm;
    int     w, h;
} UndoEntry;

/* ── App state (single global) ────────────────────────────────────── */
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
    int      zoomAnchorX, zoomAnchorY; /* screen pt for zoom anim     */

    /* folder navigation — dynamically allocated */
    WCHAR  **dirFiles;      /* heap array of MAX_PATH-wide strings     */
    int      dirCount;
    int      dirCap;        /* allocated capacity                      */
    int      dirIndex;

    /* slideshow */
    BOOL            slideshowOn;
    SlideshowSpeed  slideshowSpeed;

    /* recent files */
    WCHAR    recent[MAX_RECENT][MAX_PATH];
    int      recentCount;

    /* thumbnails — dynamically allocated parallel to dirFiles */
    Thumb   *thumbs;
    int      thumbCount;
    BOOL     thumbVisible;
    int      thumbScroll;

    /* transform undo stack */
    UndoEntry undo[UNDO_DEPTH];
    int       undoCount;

    /* UI state */
    BOOL     fullscreen;
    RECT     prevWinRect;
    DWORD    prevStyle;
    BOOL     infoVisible;

    /* image metadata text */
    WCHAR    infoText[INFO_TEXT_LEN];

    HWND      hwnd;
    HINSTANCE hInst;
} AppState;

extern AppState g_app;

/* ── Toolbar button descriptor (shared between draw.c and main.c) ─── */
typedef struct { int x; int w; int cmd; const WCHAR *label; const WCHAR *tip; } TBBtn;
extern TBBtn    g_btns[];
extern int      g_btnCount;

/* ════════════════════════════════════════════════════════════════════
   Function declarations
   ════════════════════════════════════════════════════════════════════ */

/* image_io.c */
Image  *img_load(const WCHAR *path);
Image  *img_clone(const Image *src);
void    img_free(Image *img);
HBITMAP img_to_hbitmap(HDC hdc, const Image *img);
BOOL    img_save(const Image *img, const WCHAR *path); /* stb_image_write */

/* transform.c */
Image  *img_rotate_cw(const Image *src);
Image  *img_rotate_ccw(const Image *src);
Image  *img_flip_h(const Image *src);
Image  *img_flip_v(const Image *src);

/* folder.c */
void    folder_scan(const WCHAR *path);
void    folder_free(void);
void    folder_go(HWND hwnd, int index);
void    folder_next(HWND hwnd);
void    folder_prev(HWND hwnd);

/* thumbs.c */
void    thumbs_sync(void);
void    thumbs_load_visible(HDC hdc, RECT panelRC);
void    thumbs_free_all(void);
void    thumbs_draw(HDC hdc, RECT clientRC);
void    thumbs_on_click(HWND hwnd, int x, int y, RECT clientRC);
void    thumbs_on_scroll(int delta, RECT clientRC);

/* recent.c */
void    recent_add(const WCHAR *path);
void    recent_rebuild_menu(HMENU hMenu);

/* ui.c */
void    ui_open_file(HWND hwnd);
void    ui_open_path(HWND hwnd, const WCHAR *path);
void    ui_save_as(HWND hwnd);
void    ui_copy_to_clipboard(HWND hwnd);
void    ui_delete_current(HWND hwnd);
void    ui_set_title(HWND hwnd);
void    ui_fit(HWND hwnd);
void    ui_reset_zoom(HWND hwnd);
void    ui_zoom_to(HWND hwnd, double nz, int cx, int cy);
void    ui_zoom_tick(HWND hwnd);
void    ui_clamp_pan(HWND hwnd);
void    ui_toggle_fullscreen(HWND hwnd);
void    ui_toggle_info(HWND hwnd);
void    ui_toggle_slideshow(HWND hwnd);
void    ui_set_slideshow_speed(HWND hwnd, SlideshowSpeed spd);
void    ui_toggle_thumbs(HWND hwnd);
void    ui_apply_transform(HWND hwnd, int kind);
void    ui_undo_transform(HWND hwnd);
void    ui_push_undo(void);
void    ui_center_image(HWND hwnd);
RECT    ui_canvas_rect(HWND hwnd);

/* draw.c */
void    draw_toolbar(HDC hdc, HWND hwnd);
void    draw_image(HDC hdc, HWND hwnd);
void    draw_info(HDC hdc, HWND hwnd);
void    draw_scene(HWND hwnd);
int     toolbar_hit(int mx, int my);
