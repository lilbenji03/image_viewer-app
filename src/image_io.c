/*
 * image_io.c  —  load, clone, save, and convert images
 *
 * Improvements over v2:
 *  - img_clone()    : deep-copy an Image (needed for undo stack)
 *  - img_save()     : export via stb_image_write (PNG / JPEG / BMP)
 *  - Better error message (no MessageBox in load helpers)
 *  - format field is now up to 32 chars (was 16)
 */
#include "../include/viewer.h"
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/* Global app state lives here */
AppState g_app = {0};

/* ── Load image from wide path ──────────────────────────────────── */
Image *img_load(const WCHAR *wpath) {
    char path_utf8[MAX_PATH * 4];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                        path_utf8, (int)sizeof(path_utf8), NULL, NULL);

    int w, h, channels;
    uint8_t *data = stbi_load(path_utf8, &w, &h, &channels, 4);
    if (!data) {
        WCHAR msg[640];
        swprintf_s(msg, 640, L"Could not open image.\n%hs", stbi_failure_reason());
        MessageBoxW(NULL, msg, L"Error", MB_ICONERROR);
        return NULL;
    }

    Image *img  = (Image *)calloc(1, sizeof(Image));
    if (!img) { stbi_image_free(data); return NULL; }

    img->w      = w;
    img->h      = h;
    img->orig_w = w;
    img->orig_h = h;
    img->px     = (Pixel *)malloc((size_t)w * h * sizeof(Pixel));
    if (!img->px) { free(img); stbi_image_free(data); return NULL; }

    memcpy(img->px, data, (size_t)w * h * 4);
    stbi_image_free(data);

    wcscpy_s(img->path, MAX_PATH, wpath);

    const WCHAR *ext = PathFindExtensionW(wpath);
    if (ext && *ext)
        wcscpy_s(img->format, 32, ext + 1);  /* skip dot */
    else
        wcscpy_s(img->format, 32, L"?");

    /* Report the original channel count as bit-depth */
    img->bit_depth = channels * 8;
    return img;
}

/* ── Deep-copy an Image (for undo stack) ────────────────────────── */
Image *img_clone(const Image *src) {
    if (!src) return NULL;
    Image *dst = (Image *)malloc(sizeof(Image));
    if (!dst) return NULL;
    *dst = *src;   /* copy all scalar fields */
    dst->px = (Pixel *)malloc((size_t)src->w * src->h * sizeof(Pixel));
    if (!dst->px) { free(dst); return NULL; }
    memcpy(dst->px, src->px, (size_t)src->w * src->h * sizeof(Pixel));
    return dst;
}

/* ── Free image ─────────────────────────────────────────────────── */
void img_free(Image *img) {
    if (!img) return;
    free(img->px);
    free(img);
}

/* ── Convert RGBA Image → HBITMAP (32-bit DIB, top-down) ────────── */
HBITMAP img_to_hbitmap(HDC hdc, const Image *img) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       =  img->w;
    bmi.bmiHeader.biHeight      = -img->h;   /* negative = top-down  */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbm || !bits) return NULL;

    /* DIB stores BGRA; Image stores RGBA */
    uint8_t       *dst = (uint8_t *)bits;
    const Pixel   *src = img->px;
    int n = img->w * img->h;
    for (int i = 0; i < n; i++) {
        dst[i*4+0] = src[i].b;
        dst[i*4+1] = src[i].g;
        dst[i*4+2] = src[i].r;
        dst[i*4+3] = src[i].a;
    }
    return hbm;
}

/* ── Save image to file (PNG / JPEG / BMP auto-detected by ext) ─── */
BOOL img_save(const Image *img, const WCHAR *wpath) {
    if (!img || !wpath) return FALSE;

    char path_utf8[MAX_PATH * 4];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1,
                        path_utf8, (int)sizeof(path_utf8), NULL, NULL);

    const WCHAR *ext = PathFindExtensionW(wpath);
    int ok = 0;

    /* Build a packed RGBA buffer for stb_image_write */
    int stride = img->w * 4;

    if (ext && _wcsicmp(ext, L".png") == 0) {
        ok = stbi_write_png(path_utf8, img->w, img->h, 4,
                            img->px, stride);
    } else if (ext && (_wcsicmp(ext, L".jpg")  == 0 ||
                       _wcsicmp(ext, L".jpeg") == 0)) {
        ok = stbi_write_jpg(path_utf8, img->w, img->h, 4, img->px, 90);
    } else if (ext && _wcsicmp(ext, L".bmp") == 0) {
        ok = stbi_write_bmp(path_utf8, img->w, img->h, 4, img->px);
    } else {
        /* Default to PNG */
        ok = stbi_write_png(path_utf8, img->w, img->h, 4,
                            img->px, stride);
    }

    if (!ok) {
        MessageBoxW(NULL, L"Failed to save image.", L"Save Error", MB_ICONERROR);
        return FALSE;
    }
    return TRUE;
}
