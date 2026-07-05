#include "app.h"
#include "main_window.h"
#include "mod_list_view.h"
#include "scanner.h"
#include "modrinth.h"
#include "cache.h"
#include <gtk/gtk.h>

static gboolean auto_scan_finished(gpointer userdata)
{
    (void)userdata;
    mod_list_view_refresh();
    main_window_update_status();
    AppState *state = app_get_state();
    state->is_scanning = FALSE;
    return G_SOURCE_REMOVE;
}

static gpointer auto_scan_thread(gpointer userdata)
{
    char *dir = (char *)userdata;
    GPtrArray *mods = mod_list_get_all();
    mod_list_clear();
    int found = scanner_scan_directory(dir, mods);

    // 无模组则跳过后续处理
    if (found > 0) {
        for (int i = 0; i < mod_list_count(); i++) {
            ModInfo *mod = mod_list_get(i);
            if (!mod) continue;
            modrinth_query_version(mod, NULL, NULL);
        }
        cache_save(mods);
    }

    g_idle_add(auto_scan_finished, NULL);
    g_free(dir);
    return NULL;
}

static gboolean auto_scan_start(gpointer userdata)
{
    (void)userdata;
    AppState *state = app_get_state();
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
