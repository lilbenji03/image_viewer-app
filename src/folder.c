/*
 * folder.c  —  scan folder for images, next/prev navigation
 *
 * Improvements over v2:
 *  - dirFiles is now a heap-allocated dynamic array (no 512-file cap)
 *  - folder_free() cleans up the heap; called on every new scan
 *  - Files are sorted alphabetically (FindFirstFile order is not stable)
 */
#include "../include/viewer.h"

static const WCHAR *EXTS[] = {
    L".jpg", L".jpeg", L".png", L".bmp",
    L".gif", L".tga",  L".ppm", L".pgm",
    L".tiff", L".tif", NULL
};

static BOOL is_image_ext(const WCHAR *name) {
    const WCHAR *ext = PathFindExtensionW(name);
    if (!ext || !*ext) return FALSE;
    for (int i = 0; EXTS[i]; i++)
        if (_wcsicmp(ext, EXTS[i]) == 0) return TRUE;
    return FALSE;
}

/* ── Free previously scanned directory ──────────────────────────── */
void folder_free(void) {
    if (g_app.dirFiles) {
        for (int i = 0; i < g_app.dirCap; i++)
            free(g_app.dirFiles[i]);
        free(g_app.dirFiles);
        g_app.dirFiles = NULL;
    }
    g_app.dirCount = 0;
    g_app.dirCap   = 0;
    g_app.dirIndex = 0;
}

/* ── qsort comparator for wide strings ──────────────────────────── */
static int wcscmp_qsort(const void *a, const void *b) {
    return _wcsicmp(*(const WCHAR **)a, *(const WCHAR **)b);
}

/* ── Scan folder containing filepath ────────────────────────────── */
void folder_scan(const WCHAR *filepath) {
    folder_free();

    WCHAR dir[MAX_PATH];
    wcscpy_s(dir, MAX_PATH, filepath);
    PathRemoveFileSpecW(dir);

    WCHAR pattern[MAX_PATH];
    swprintf_s(pattern, MAX_PATH, L"%s\\*", dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    /* Initial capacity */
    int cap = 64;
    WCHAR **arr = (WCHAR **)malloc((size_t)cap * sizeof(WCHAR *));
    if (!arr) { FindClose(hFind); return; }
    int count = 0;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_image_ext(fd.cFileName))                    continue;

        /* Grow if needed */
        if (count >= cap) {
            int newcap = cap * 2;
            WCHAR **tmp = (WCHAR **)realloc(arr, (size_t)newcap * sizeof(WCHAR *));
            if (!tmp) break;
            arr = tmp;
            cap = newcap;
        }

        arr[count] = (WCHAR *)malloc(MAX_PATH * sizeof(WCHAR));
        if (!arr[count]) break;
        swprintf_s(arr[count], MAX_PATH, L"%s\\%s", dir, fd.cFileName);
        count++;

    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    /* Sort alphabetically */
    qsort(arr, (size_t)count, sizeof(WCHAR *), wcscmp_qsort);

    g_app.dirFiles = arr;
    g_app.dirCount = count;
    g_app.dirCap   = cap;

    /* Find which index is the current file */
    for (int i = 0; i < count; i++) {
        if (_wcsicmp(arr[i], filepath) == 0) {
            g_app.dirIndex = i;
            break;
        }
    }
}

/* ── Navigate to index (wraps around) ───────────────────────────── */
void folder_go(HWND hwnd, int index) {
    if (g_app.dirCount == 0) return;
    if (index < 0)                 index = g_app.dirCount - 1;
    if (index >= g_app.dirCount)   index = 0;
    g_app.dirIndex = index;
    ui_open_path(hwnd, g_app.dirFiles[index]);
}

void folder_next(HWND hwnd) { folder_go(hwnd, g_app.dirIndex + 1); }
void folder_prev(HWND hwnd) { folder_go(hwnd, g_app.dirIndex - 1); }
