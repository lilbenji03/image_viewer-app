/*
 * transform.c  —  in-memory rotate and flip operations
 *
 * Improvements over v2:
 *  - All functions take `const Image *src` (callers must not modify src)
 *  - NULL-safe: if allocation fails, returns NULL cleanly
 *  - Helper img_alloc() checks malloc result
 *  - Transforms now stack correctly because ui_apply_transform() works
 *    on the live in-memory pixel data (see ui.c), not disk re-loads
 */
#include "../include/viewer.h"

/* ── Internal allocator ─────────────────────────────────────────── */
static Image *img_alloc(int w, int h) {
    Image *out = (Image *)calloc(1, sizeof(Image));
    if (!out) return NULL;
    out->w  = w;
    out->h  = h;
    out->px = (Pixel *)malloc((size_t)w * h * sizeof(Pixel));
    if (!out->px) { free(out); return NULL; }
    return out;
}

/* ── Copy metadata from src to dst ─────────────────────────────── */
static void copy_meta(Image *dst, const Image *src) {
    dst->bit_depth = src->bit_depth;
    dst->orig_w    = src->orig_w;
    dst->orig_h    = src->orig_h;
    dst->flip_h    = src->flip_h;
    dst->flip_v    = src->flip_v;
    wcscpy_s(dst->path,   MAX_PATH, src->path);
    wcscpy_s(dst->format, 32,       src->format);
}

/* ── Rotate 90° clockwise ───────────────────────────────────────── */
Image *img_rotate_cw(const Image *src) {
    Image *dst = img_alloc(src->h, src->w);
    if (!dst) return NULL;
    copy_meta(dst, src);
    dst->rotation = (src->rotation + 90) % 360;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[x * dst->w + (src->h - 1 - y)] = src->px[y * src->w + x];
    return dst;
}

/* ── Rotate 90° counter-clockwise ───────────────────────────────── */
Image *img_rotate_ccw(const Image *src) {
    Image *dst = img_alloc(src->h, src->w);
    if (!dst) return NULL;
    copy_meta(dst, src);
    dst->rotation = (src->rotation + 270) % 360;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[(src->w - 1 - x) * dst->w + y] = src->px[y * src->w + x];
    return dst;
}

/* ── Flip horizontal ────────────────────────────────────────────── */
Image *img_flip_h(const Image *src) {
    Image *dst = img_alloc(src->w, src->h);
    if (!dst) return NULL;
    copy_meta(dst, src);
    dst->rotation = src->rotation;
    dst->flip_h   = !src->flip_h;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[y * src->w + (src->w - 1 - x)] = src->px[y * src->w + x];
    return dst;
}

/* ── Flip vertical ──────────────────────────────────────────────── */
Image *img_flip_v(const Image *src) {
    Image *dst = img_alloc(src->w, src->h);
    if (!dst) return NULL;
    copy_meta(dst, src);
    dst->rotation = src->rotation;
    dst->flip_v   = !src->flip_v;

    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++)
            dst->px[(src->h - 1 - y) * src->w + x] = src->px[y * src->w + x];
    return dst;
}
