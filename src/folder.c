/*
 * folder.c  —  scan folder for images, next/prev navigation
 */
#include "../include/viewer.h"

static const WCHAR *EXTS[] = {
    L".jpg", L".jpeg", L".png", L".bmp",
    L".gif",  L".tga",  L".ppm", L".pgm", L".tiff", L".tif", NULL
};

static BOOL is_image(const WCHAR *name) {
    const WCHAR *ext = PathFindExtensionW(name);
    if (!ext || !*ext) return FALSE;
    for (int i = 0; EXTS[i]; i++) {
        if (_wcsicmp(ext, EXTS[i]) == 0) return TRUE;
    }
    return FALSE;
}

void folder_scan(const WCHAR *filepath) {
    /* Derive directory from file path */
    WCHAR dir[MAX_PATH];
    wcscpy_s(dir, MAX_PATH, filepath);
    PathRemoveFileSpecW(dir);

    g_app.dirCount = 0;
    g_app.dirIndex = 0;

    WCHAR pattern[MAX_PATH];
    swprintf_s(pattern, MAX_PATH, L"%s\\*", dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!is_image(fd.cFileName)) continue;
        if (g_app.dirCount >= MAX_DIR_FILES) break;

        swprintf_s(g_app.dirFiles[g_app.dirCount], MAX_PATH,
                   L"%s\\%s", dir, fd.cFileName);

        /* Track which index is the current file */
        if (_wcsicmp(g_app.dirFiles[g_app.dirCount], filepath) == 0)
            g_app.dirIndex = g_app.dirCount;

        g_app.dirCount++;
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

void folder_go(HWND hwnd, int index) {
    if (g_app.dirCount == 0) return;
    /* Wrap around */
    if (index < 0) index = g_app.dirCount - 1;
    if (index >= g_app.dirCount) index = 0;
    g_app.dirIndex = index;
    ui_open_path(hwnd, g_app.dirFiles[index]);
}

void folder_next(HWND hwnd) { folder_go(hwnd, g_app.dirIndex + 1); }
void folder_prev(HWND hwnd) { folder_go(hwnd, g_app.dirIndex - 1); }
