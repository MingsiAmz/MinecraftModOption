#include "app.h"
#include "main_window.h"
#include <gtk/gtk.h>

/* 启动后自动扫描：逻辑与「扫描」按钮完全一致，避免两套实现 */
static gboolean auto_scan_start(gpointer userdata)
{
    (void)userdata;
    main_window_start_scan();
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
