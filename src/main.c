#include "app.h"
#include "main_window.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* 便携版运行时：GtkFileDialog 在 Windows 上走原生 IFileDialog，会读写
 * org.gtk.gtk4.Settings.FileChooser 这一 GSettings schema。GLib 默认只在
 * 安装前缀下查找 schema，而免安装包是平铺布局（DLL 与 exe 同目录），
 * 找不到 schema 时 GLib 直接 g_error 中止进程 —— 表现就是点「浏览…」
 * 或工具栏目录按钮时闪退。这里在 gtk_init() 之前把 exe 同级的
 * share/glib-2.0/schemas 指给 GLib；文件不存在（如开发机上直接用
 * MSYS2 运行时）则什么都不做，保持原有行为。 */
#ifdef _WIN32
static void setup_portable_schemas(void)
{
    if (g_getenv("GSETTINGS_SCHEMA_DIR"))
        return;

    char exe_dir[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
    if (len == 0 || len >= sizeof(exe_dir))
        return;

    char *last_sep = strrchr(exe_dir, '\\');
    if (!last_sep)
        return;
    *last_sep = '\0';

    char schema_dir[MAX_PATH];
    int n = snprintf(schema_dir, sizeof(schema_dir), "%s\\share\\glib-2.0\\schemas", exe_dir);
    if (n <= 0 || (size_t)n >= sizeof(schema_dir))
        return;

    char compiled[MAX_PATH];
    n = snprintf(compiled, sizeof(compiled), "%s\\gschemas.compiled", schema_dir);
    if (n <= 0 || (size_t)n >= sizeof(compiled))
        return;

    if (GetFileAttributesA(compiled) == INVALID_FILE_ATTRIBUTES)
        return;

    g_setenv("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);
}
#endif

/* 启动后自动扫描：逻辑与「扫描」按钮完全一致，避免两套实现 */
static gboolean auto_scan_start(gpointer userdata)
{
    (void)userdata;
    main_window_start_scan();
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    setup_portable_schemas();
#endif

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
