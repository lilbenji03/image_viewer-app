/*
 * recent.c  —  recent files list (in-memory, MAX_RECENT entries)
 *
 * Improvements over v2:
 *  - MAX_RECENT raised to 20 (defined in viewer.h)
 *  - recent_rebuild_menu() shows full path as tooltip-like text
 *    and uses & accelerator digits 1–9, then letters a–k
 */
#include "../include/viewer.h"

/* ── Add a path to the front of the recent list ─────────────────── */
void recent_add(const WCHAR *path) {
    /* Remove duplicate if already present */
    for (int i = 0; i < g_app.recentCount; i++) {
        if (_wcsicmp(g_app.recent[i], path) == 0) {
            for (int j = i; j < g_app.recentCount - 1; j++)
                wcscpy_s(g_app.recent[j], MAX_PATH, g_app.recent[j + 1]);
            g_app.recentCount--;
            break;
        }
    }

    /* Shift everything down, insert at front */
    int cap = (g_app.recentCount < MAX_RECENT)
              ? g_app.recentCount : MAX_RECENT - 1;
    for (int i = cap; i > 0; i--)
        wcscpy_s(g_app.recent[i], MAX_PATH, g_app.recent[i - 1]);
    wcscpy_s(g_app.recent[0], MAX_PATH, path);
    if (g_app.recentCount < MAX_RECENT) g_app.recentCount++;
}

/* ── Rebuild the Recent Files submenu ───────────────────────────── */
void recent_rebuild_menu(HMENU hMenu) {
    /* Remove old entries */
    for (int i = 0; i < MAX_RECENT; i++)
        RemoveMenu(hMenu, IDM_RECENT_BASE + i, MF_BYCOMMAND);

    if (g_app.recentCount == 0) {
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED,
                    IDM_RECENT_BASE, L"(none)");
        return;
    }

    for (int i = 0; i < g_app.recentCount; i++) {
        const WCHAR *slash = wcsrchr(g_app.recent[i], L'\\');
        const WCHAR *name  = slash ? slash + 1 : g_app.recent[i];

        /* Accelerator: &1 .. &9, then &a .. */
        WCHAR accel[4];
        if (i < 9) swprintf_s(accel, 4, L"&%d", i + 1);
        else        swprintf_s(accel, 4, L"&%c", L'a' + (i - 9));

        WCHAR label[MAX_PATH + 8];
        swprintf_s(label, MAX_PATH + 8, L"%s  %s", accel, name);
        AppendMenuW(hMenu, MF_STRING, IDM_RECENT_BASE + i, label);
    }
}
