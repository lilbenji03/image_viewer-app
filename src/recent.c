/*
 * recent.c  —  recent files list (in-memory, max MAX_RECENT)
 */
#include "../include/viewer.h"

void recent_add(const WCHAR *path) {
    /* Remove if already present */
    for (int i = 0; i < g_app.recentCount; i++) {
        if (_wcsicmp(g_app.recent[i], path) == 0) {
            /* Shift down */
            for (int j = i; j < g_app.recentCount - 1; j++)
                wcscpy_s(g_app.recent[j], MAX_PATH, g_app.recent[j+1]);
            g_app.recentCount--;
            break;
        }
    }
    /* Shift up and insert at front */
    int cap = g_app.recentCount < MAX_RECENT ? g_app.recentCount : MAX_RECENT - 1;
    for (int i = cap; i > 0; i--)
        wcscpy_s(g_app.recent[i], MAX_PATH, g_app.recent[i-1]);
    wcscpy_s(g_app.recent[0], MAX_PATH, path);
    if (g_app.recentCount < MAX_RECENT) g_app.recentCount++;
}

void recent_rebuild_menu(HMENU hMenu) {
    /* Find "Recent Files" submenu — it's a popup under File */
    /* We store it as items IDM_RECENT_BASE .. IDM_RECENT_BASE+MAX_RECENT-1 */
    /* Remove old items */
    for (int i = 0; i < MAX_RECENT; i++)
        RemoveMenu(hMenu, IDM_RECENT_BASE + i, MF_BYCOMMAND);

    if (g_app.recentCount == 0) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, IDM_RECENT_BASE, L"(none)");
        return;
    }
    for (int i = 0; i < g_app.recentCount; i++) {
        /* Show only filename, not full path */
        const WCHAR *slash = wcsrchr(g_app.recent[i], L'\\');
        WCHAR label[MAX_PATH + 4];
        swprintf_s(label, MAX_PATH + 4, L"&%d  %s", i + 1,
                   slash ? slash + 1 : g_app.recent[i]);
        AppendMenuW(hMenu, MF_STRING, IDM_RECENT_BASE + i, label);
    }
}
