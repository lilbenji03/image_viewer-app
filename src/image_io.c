/*
 * image_io.c  —  load any image format via stb_image
 * Supports: JPEG, PNG, BMP, GIF (first frame), TGA, PNM, TIFF (via stb)
 */
#include "../include/viewer.h"
#include "stb_image.h"

AppState g_app = {0};

/* ── Load image from wide path ──────────────────────────────────── */
Image *img_load(const WCHAR *wpath) {
    /* Convert wide path → UTF-8 for stb_image */
    char path_utf8[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path_utf8, sizeof(path_utf8), NULL, NULL);

    int w, h, channels;
    uint8_t *data = stbi_load(path_utf8, &w, &h, &channels, 4); /* force RGBA */
    if (!data) {
        WCHAR msg[512];
        swprintf_s(msg, 512, L"Cannot open image:\n%s\n\n%hs",
                   wpath, stbi_failure_reason());
        MessageBoxW(NULL, msg, L"Load Error", MB_ICONERROR);
        return NULL;
    }

    Image *img = (Image *)calloc(1, sizeof(Image));
    img->w = w; img->h = h;
    img->orig_w = w; img->orig_h = h;
    img->px = (Pixel *)malloc((size_t)w * h * sizeof(Pixel));
    memcpy(img->px, data, (size_t)w * h * 4);
    stbi_image_free(data);

    /* Store path & format */
    wcscpy_s(img->path, MAX_PATH, wpath);
    const WCHAR *ext = PathFindExtensionW(wpath);
    if (ext) wcscpy_s(img->format, 16, ext + 1);  /* skip dot */
    else     wcscpy_s(img->format, 16, L"?");

    /* Bit depth: channels * 8 */
    img->bit_depth = channels * 8;

    return img;
}

/* ── Free image ─────────────────────────────────────────────────── */
void img_free(Image *img) {
    if (!img) return;
    free(img->px);
    free(img);
}

/* ── Convert RGBA Image → HBITMAP (32-bit DIB) ──────────────────── */
HBITMAP img_to_hbitmap(HDC hdc, const Image *img) {
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       =  img->w;
    bmi.bmiHeader.biHeight      = -img->h;  /* top-down */
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbm || !bits) return NULL;

    /* DIB stores BGRA; our Pixel is RGBA */
    uint8_t *dst = (uint8_t *)bits;
    const Pixel *src = img->px;
    int n = img->w * img->h;
    for (int i = 0; i < n; i++) {
        dst[i*4+0] = src[i].b;
        dst[i*4+1] = src[i].g;
        dst[i*4+2] = src[i].r;
        dst[i*4+3] = src[i].a;
    }
    return hbm;
}
