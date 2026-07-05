#include "app.h"
#include "main_window.h"
#include "mod_list_view.h"
#include "scanner.h"
#include "modrinth.h"
#include "cache.h"
#include <gtk/gtk.h>
#include <windows.h>
#include <string.h>

// ─── 写入调试日志（exe 同目录） ───
static void write_debug_log(const char *fname, const char *fmt, ...)
{
    char path[2048];
    GetModuleFileNameA(NULL, path, sizeof(path));
    char *p = strrchr(path, '\\');
    if (p) *(p+1) = '\0';
    else path[0] = '\0';
    strncat(path, fname, sizeof(path) - strlen(path) - 1);

    FILE *f = fopen(path, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

static gboolean auto_scan_finished(gpointer userdata)
{
    (void)userdata;
    write_debug_log("scan_log.txt", "auto_scan_finished: START, mod_list_count=%d\n", mod_list_count());
    mod_list_view_refresh();
    main_window_update_status();
    AppState *state = app_get_state();
    state->is_scanning = FALSE;
    write_debug_log("scan_log.txt", "auto_scan_finished: DONE\n");
    return G_SOURCE_REMOVE;
}

static gpointer auto_scan_thread(gpointer userdata)
{
    char *dir = (char *)userdata;

    write_debug_log("scan_log.txt", "=== auto_scan_thread STARTED ===\ndir=%s\n", dir);

    // 用 FindFirstFile 直接搜 .jar 文件（不解析 jar 内部）
    mod_list_clear();

    char pattern[1030];
    snprintf(pattern, sizeof(pattern), "%s\\*.jar", dir);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    int jar_count = 0;

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s\\%s", dir, ffd.cFileName);

            ModInfo info;
            memset(&info, 0, sizeof(info));
            snprintf(info.file_name, sizeof(info.file_name), "%s", ffd.cFileName);
            snprintf(info.file_path, sizeof(info.file_path), "%s", full_path);
            info.file_size = (long)(ffd.nFileSizeLow);
            info.status = MOD_STATUS_UNKNOWN;

            // 文件名去 .jar 后缀
            char name_only[256];
            snprintf(name_only, sizeof(name_only), "%s", ffd.cFileName);
            char *dot = strrchr(name_only, '.');
            if (dot && (strcmp(dot, ".jar") == 0 ||
                strcmp(dot, ".JAR") == 0 ||
                (dot[0]=='.' && (dot[1]=='j'||dot[1]=='J') &&
                              (dot[2]=='a'||dot[2]=='A') &&
                              (dot[3]=='r'||dot[3]=='R'))))
                *dot = '\0';
            snprintf(info.name, sizeof(info.name), "%s", name_only);
            snprintf(info.mod_id, sizeof(info.mod_id), "%s", name_only);

            mod_list_add(&info);
            jar_count++;
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }

    write_debug_log("scan_log.txt", "auto_scan: jars found=%d, mod_list_count=%d\n", jar_count, mod_list_count());

    // 立即刷新 UI 显示列表
    g_idle_add(auto_scan_finished, NULL);

    // 后台版本查询
    if (jar_count > 0) {
        GPtrArray *mods = mod_list_get_all();
        for (int i = 0; i < mod_list_count(); i++) {
            ModInfo *mod = mod_list_get(i);
            if (!mod) continue;
            modrinth_query_version(mod, NULL, NULL);
        }
        cache_save(mods);
    }

    g_free(dir);
    return NULL;
}

static gboolean auto_scan_start(gpointer userdata)
{
    (void)userdata;
    AppState *state = app_get_state();
    write_debug_log("scan_log.txt", "auto_scan_start: is_scanning=%d\n", state->is_scanning);
    if (!state->is_scanning) {
        state->is_scanning = TRUE;
        g_thread_new("auto-scan", auto_scan_thread,
            g_strdup(state->config.mod_dir));
    }
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    gtk_init();

    if (app_init() != 0) {
        g_critical("App init failed");
        return 1;
    }

    GtkWidget *win = main_window_create();
    if (!win) {
        app_cleanup();
        return 1;
    }

    gtk_window_present(GTK_WINDOW(win));

    AppState *state = app_get_state();
    if (state->config.auto_scan_on_start) {
        g_idle_add(auto_scan_start, NULL);
    }

    // 在 GTK4 中，没有 gtk_main()，使用 GLib 主循环
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_signal_connect_swapped(win, "destroy", G_CALLBACK(g_main_loop_quit), loop);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    app_cleanup();
    return 0;
}
