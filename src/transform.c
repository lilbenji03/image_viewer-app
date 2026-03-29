/*
 * transform.c  —  rotate and flip operations
 */
#include "../include/viewer.h"

static Image *img_alloc(int w, int h) {
    Image *out = (Image *)calloc(1, sizeof(Image));
    out->w = w; out->h = h;
    out->px = (Pixel *)malloc((size_t)w * h * sizeof(Pixel));
    return out;
}

/* ── Rotate 90° clockwise ───────────────────────────────────────── */
Image *img_rotate_cw(Image *src) {
    Image *dst = img_alloc(src->h, src->w);
    dst->bit_depth = src->bit_depth;
    wcscpy_s(dst->path,   MAX_PATH, src->path);
    wcscpy_s(dst->format, 16,       src->format);
    dst->orig_w = src->orig_w; dst->orig_h = src->orig_h;
    dst->rotation = (src->rotation + 90) % 360;
    dst->flip_h = src->flip_h; dst->flip_v = src->flip_v;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[x * dst->w + (src->h - 1 - y)] = src->px[y * src->w + x];
    return dst;
}

/* ── Rotate 90° counter-clockwise ───────────────────────────────── */
Image *img_rotate_ccw(Image *src) {
    Image *dst = img_alloc(src->h, src->w);
    dst->bit_depth = src->bit_depth;
    wcscpy_s(dst->path,   MAX_PATH, src->path);
    wcscpy_s(dst->format, 16,       src->format);
    dst->orig_w = src->orig_w; dst->orig_h = src->orig_h;
    dst->rotation = (src->rotation + 270) % 360;
    dst->flip_h = src->flip_h; dst->flip_v = src->flip_v;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[(src->w - 1 - x) * dst->w + y] = src->px[y * src->w + x];
    return dst;
}

/* ── Flip horizontal ────────────────────────────────────────────── */
Image *img_flip_h(Image *src) {
    Image *dst = img_alloc(src->w, src->h);
    dst->bit_depth = src->bit_depth;
    wcscpy_s(dst->path,   MAX_PATH, src->path);
    wcscpy_s(dst->format, 16,       src->format);
    dst->orig_w = src->orig_w; dst->orig_h = src->orig_h;
    dst->rotation = src->rotation;
    dst->flip_h = !src->flip_h; dst->flip_v = src->flip_v;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[y * src->w + (src->w - 1 - x)] = src->px[y * src->w + x];
    return dst;
}

/* ── Flip vertical ──────────────────────────────────────────────── */
Image *img_flip_v(Image *src) {
    Image *dst = img_alloc(src->w, src->h);
    dst->bit_depth = src->bit_depth;
    wcscpy_s(dst->path,   MAX_PATH, src->path);
    wcscpy_s(dst->format, 16,       src->format);
    dst->orig_w = src->orig_w; dst->orig_h = src->orig_h;
    dst->rotation = src->rotation;
    dst->flip_h = src->flip_h; dst->flip_v = !src->flip_v;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[(src->h - 1 - y) * src->w + x] = src->px[y * src->w + x];
    return dst;
}
