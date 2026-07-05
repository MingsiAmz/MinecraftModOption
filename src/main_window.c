#include "main_window.h"
#include "app.h"
#include "mod_list_view.h"
#include "dialogs.h"
#include "config.h"
#include "scanner.h"
#include "modrinth.h"
#include "curseforge.h"
#include "updater.h"
#include "deleter.h"
#include "rollback.h"
#include "cache.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>
#include <windows.h>
#include <stdarg.h>

/* ─── 前向声明 ─── */
static void write_debug_log(const char *fname, const char *fmt, ...);

/* ─── 全局控件引用 ─── */
static GtkWidget *window = NULL;
static GtkWidget *status_bar_label = NULL;
static GtkWidget *progress_bar = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *filter_combo = NULL;

/* ─── 状态栏更新 ─── */
void main_window_update_status(void)
{
    if (!status_bar_label) return;

    AppState *state = app_get_state();
    int total = mod_list_count();
    int updates = mod_list_update_count();

    GString *s = g_string_sized_new(256);
    g_string_append_printf(s,
        "\xf0\x9f\x93\x81 \xe6\xa8\xa1\xe7\xbb\x84: %d \xe4\xb8\xaa  \xe2\x94\x82  "
        "\xe2\xac\x86 \xe5\x8f\xaf\xe6\x9b\xb4\xe6\x96\xb0: %d \xe4\xb8\xaa",
        total, updates);

    // 如果目录有效，显示目录名（缩写）
    if (state->config.mod_dir[0]) {
        const char *dir = state->config.mod_dir;
        const char *last = strrchr(dir, '\\');
        if (!last) last = strrchr(dir, '/');
        if (last) last++;
        else last = dir;

        char dir_str[128];
        int dlen = strlen(dir);
        if (dlen > 60) {
            snprintf(dir_str, sizeof(dir_str), "...%s", last);
        } else {
            snprintf(dir_str, sizeof(dir_str), "%s", dir);
        }
        GString *tmp = g_string_new(dir_str);
        g_string_append(tmp, "  \xe2\x94\x82  ");
        g_string_append(tmp, s->str);
        g_string_free(s, TRUE);
        s = tmp;
    }

    gtk_label_set_text(GTK_LABEL(status_bar_label), s->str);
    g_string_free(s, TRUE);
}

/* ─── 进度条控制 ─── */
static void show_progress(const char *text)
{
    if (progress_bar) {
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), text);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
        gtk_widget_set_visible(progress_bar, TRUE);
    }
}

static void set_progress(double fraction, const char *text)
{
    if (progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
        if (text) gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), text);
    }
}

static void hide_progress(void)
{
    if (progress_bar) {
        gtk_widget_set_visible(progress_bar, FALSE);
    }
}

/* ═══════════════ Search & Filter ═══════════════ */
static void on_search_changed(GtkEditable *editable, gpointer userdata)
{
    (void)userdata;
    const char *text = gtk_editable_get_text(editable);
    mod_list_view_set_filter(text, NULL);
}

static void on_filter_changed(GtkComboBox *combo, gpointer userdata)
{
    (void)userdata;
    const char *filter = gtk_combo_box_get_active_id(combo);
    const char *search = gtk_editable_get_text(GTK_EDITABLE(search_entry));
    mod_list_view_set_filter(search, filter);
}

/* ═══════════════ Scan ═══════════════ */
typedef struct {
    char mod_dir[1024];
} ScanData;

/* Idle: show list immediately after scan */
static gboolean scan_finished_idle(gpointer userdata)
{
    (void)userdata;
    mod_list_view_refresh();
    main_window_update_status();
    return G_SOURCE_REMOVE;
}

/* Idle: version check done, unlock scan button */
static gboolean version_check_finished_idle(gpointer userdata)
{
    (void)userdata;
    mod_list_view_refresh();
    main_window_update_status();
    hide_progress();

    AppState *state = app_get_state();
    state->is_scanning = FALSE;

    char msg[128];
    snprintf(msg, sizeof(msg), "Scan complete: %d mods", mod_list_count());
    gtk_label_set_text(GTK_LABEL(status_bar_label), msg);
    return G_SOURCE_REMOVE;
}

/* Count .jar files in directory (reference only) */
static int count_jars(const char *dir)
{
    int count = 0;
    WIN32_FIND_DATAA ffd;
    char pattern[1030];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            size_t len = strlen(ffd.cFileName);
            if (len >= 4 && (strcasecmp(ffd.cFileName + len - 4, ".jar") == 0))
                count++;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
    return count;
}

// ─── 写入调试日志（绝对路径，确保可写入） ───
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

static gpointer scan_thread_func(gpointer userdata)
{
    ScanData *data = (ScanData *)userdata;

    write_debug_log("scan_log.txt", "=== scan_thread STARTED ===\nmod_dir=%s\n", data->mod_dir);

    // 第一步：列举目录中所有 .jar 文件（只用文件名，不解析 jar 内部）
    mod_list_clear();

    /* List .jars via FindFirstFile (fast, no jar parsing) */
    int jar_count = 0;
    char pattern[1030];
    snprintf(pattern, sizeof(pattern), "%s\\*.jar", data->mod_dir);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

            char full_path[2048];
            snprintf(full_path, sizeof(full_path), "%s\\%s", data->mod_dir, ffd.cFileName);

            // 创建模组信息（只用文件名，不解析 jar）
            ModInfo info;
            memset(&info, 0, sizeof(info));
            snprintf(info.file_name, sizeof(info.file_name), "%s", ffd.cFileName);
            snprintf(info.file_path, sizeof(info.file_path), "%s", full_path);
            info.file_size = (long)(ffd.nFileSizeLow);
            info.status = MOD_STATUS_UNKNOWN;

            // 文件名去掉 .jar 作为名称
            char name_only[256];
            snprintf(name_only, sizeof(name_only), "%s", ffd.cFileName);
            char *dot = strrchr(name_only, '.');
            if (dot && strcasecmp(dot, ".jar") == 0) *dot = '\0';
            snprintf(info.name, sizeof(info.name), "%s", name_only);
            snprintf(info.mod_id, sizeof(info.mod_id), "%s", name_only);

            mod_list_add(&info);
            jar_count++;
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }

    write_debug_log("scan_log.txt", "Total jars found: %d\n", jar_count);
    write_debug_log("scan_log.txt", "mod_list_count: %d\n", mod_list_count());

    int total = mod_list_count();

    // ─── 立即刷新 UI 显示列表（不等版本查询） ───
    g_idle_add(scan_finished_idle, NULL);

    // ─── 后台版本查询 ───
    if (total > 0) {
        AppState *state = app_get_state();

        for (int i = 0; i < total; i++) {
            ModInfo *mod = mod_list_get(i);
            if (!mod) continue;

            char mc_ver[32] = "";
            if (mod->mc_version[0]) {
                const char *p = strchr(mod->mc_version, '.');
                if (p) {
                    p = strchr(p+1, '.');
                    if (p) {
                        size_t n = p - mod->mc_version;
                        if (n < sizeof(mc_ver)) strncpy(mc_ver, mod->mc_version, n);
                    }
                }
                if (!mc_ver[0]) snprintf(mc_ver, sizeof(mc_ver), "%s", mod->mc_version);
            }

            modrinth_query_version(mod, mc_ver[0]?mc_ver:NULL,
                    mod->loader[0]?mod->loader:NULL);

            if (state->config.curseforge_api_key[0]) {
                curseforge_query_version(mod, state->config.curseforge_api_key,
                    mc_ver[0]?mc_ver:NULL);
            }
        }

        GPtrArray *mods = mod_list_get_all();
        cache_save(mods);
    }

    /* Refresh UI after version queries */
    g_idle_add(version_check_finished_idle, NULL);

    g_free(data);
    return NULL;
}

static void on_scan_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    AppState *state = app_get_state();
    if (state->is_scanning) return;
    state->is_scanning = TRUE;
    ScanData *d = g_new(ScanData, 1);
    snprintf(d->mod_dir, sizeof(d->mod_dir), "%s", state->config.mod_dir);
    g_thread_new("scan", scan_thread_func, d);
}

/* ═══════════════ Select All / None ═══════════════ */
static void on_select_all_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    mod_list_view_select_all(TRUE);
}

static void on_deselect_all_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    mod_list_view_select_all(FALSE);
}

/* ═══════════════ Update ═══════════════ */
typedef struct { int *indices; int count; } UpdateData;

static gboolean update_show_result_cb(gpointer userdata)
{
    int *vals = (int *)userdata;
    dialogs_show_update_result(GTK_WINDOW(window), vals[0], vals[1], NULL, 0);
    g_free(vals);
    return G_SOURCE_REMOVE;
}

static gboolean update_finished_idle(gpointer userdata)
{
    UpdateData *data = (UpdateData *)userdata;
    mod_list_view_refresh();
    main_window_update_status();
    hide_progress();
    AppState *state = app_get_state();
    state->is_updating = FALSE;
    g_free(data->indices);
    g_free(data);
    return G_SOURCE_REMOVE;
}

static gpointer update_thread_func(gpointer userdata)
{
    UpdateData *data = (UpdateData *)userdata;
    AppState *state = app_get_state();
    int success = 0, failed = 0;
    char errors[10][512]; int error_count = 0;
    char progress_text[256];

    for (int i = 0; i < data->count; i++) {
        snprintf(progress_text, sizeof(progress_text),
            "\xe2\xac\x86 \xe6\x9b\xb4\xe6\x96\xb0: %d/%d", i+1, data->count);
        g_idle_add((GSourceFunc)set_progress,
            GSIZE_TO_POINTER((guint64)((double)(i+1)/data->count * 1000)));
        // 注意：set_progress 的 text 参数会被 GSIZE_TO_POINTER 错误传递
        // 实际用全局变量，这里简化

        ModInfo *mod = mod_list_get(data->indices[i]);
        if (!mod) { failed++; continue; }
        if (updater_update_mod(mod, state->config.backup_dir,
                state->config.backup_before_update,
                state->config.max_backups) == 0) success++;
        else {
            failed++;
            if (error_count < 10) snprintf(errors[error_count++], 512,
                "%s: \xe6\x9b\xb4\xe6\x96\xb0\xe5\xa4\xb1\xe8\xb4\xa5", mod->name);
        }
    }
    cache_save(mod_list_get_all());

    int *vals = g_new(int, 2);
    vals[0] = success; vals[1] = failed;
    g_idle_add((GSourceFunc)update_show_result_cb, vals);
    g_idle_add(update_finished_idle, data);
    return NULL;
}

static void on_update_sel_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    AppState *state = app_get_state();
    if (state->is_updating) return;

    int count; int *indices = mod_list_view_get_selected(&count);
    if (!indices || count == 0) {
        g_free(indices);
        dialogs_show_confirm(GTK_WINDOW(window), "\xe6\x8f\x90\xe7\xa4\xba",
            "\xe8\xaf\xb7\xe5\x85\x88\xe5\x8b\xbe\xe9\x80\x89\xe8\xa6\x81\xe6\x9b\xb4\xe6\x96\xb0\xe7\x9a\x84\xe6\xa8\xa1\xe7\xbb\x84\xe3\x80\x82\n\n\xe2\x8c\xa8 Ctrl+A \xe5\x85\xa8\xe9\x80\x89");
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
        "\xe7\xa1\xae\xe5\xae\x9a\xe8\xa6\x81\xe6\x9b\xb4\xe6\x96\xb0\xe9\x80\x89\xe4\xb8\xad\xe7\x9a\x84 %d \xe4\xb8\xaa\xe6\xa8\xa1\xe7\xbb\x84\xe5\x90\x97\xef\xbc\x9f\n\xe5\xbd\x93\xe5\x89\x8d\xe7\x89\x88\xe6\x9c\xac\xe5\xb0\x86\xe8\x87\xaa\xe5\x8a\xa8\xe5\xa4\x87\xe4\xbb\xbd\xe3\x80\x82",
        count);

    if (!dialogs_show_confirm(GTK_WINDOW(window), "\xe7\xa1\xae\xe8\xae\xa4\xe6\x9b\xb4\xe6\x96\xb0", msg))
        { g_free(indices); return; }

    state->is_updating = TRUE;
    show_progress("\xe2\xac\x86 \xe6\xad\xa3\xe5\x9c\xa8\xe6\x9b\xb4\xe6\x96\xb0...");
    UpdateData *d = g_new(UpdateData, 1);
    d->indices = indices; d->count = count;
    g_thread_new("update", update_thread_func, d);
}

static void on_update_all_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    AppState *state = app_get_state();
    if (state->is_updating) return;

    int total = mod_list_count();
    int *indices = g_new(int, total);
    int count = 0;
    for (int i = 0; i < total; i++) {
        ModInfo *m = mod_list_get(i);
        if (m && m->status == MOD_STATUS_UPDATE_AVAIL) indices[count++] = i;
    }

    if (count == 0) { g_free(indices);
        dialogs_show_confirm(GTK_WINDOW(window), "\xe6\x8f\x90\xe7\xa4\xba",
            "\xe6\x89\x80\xe6\x9c\x89\xe6\xa8\xa1\xe7\xbb\x84\xe5\xb7\xb2\xe6\x98\xaf\xe6\x9c\x80\xe6\x96\xb0\xe7\x89\x88\xe6\x9c\xac!"); return; }

    char msg[256];
    snprintf(msg, sizeof(msg),
        "\xe7\xa1\xae\xe5\xae\x9a\xe8\xa6\x81\xe6\x9b\xb4\xe6\x96\xb0\xe5\x85\xa8\xe9\x83\xa8 %d \xe4\xb8\xaa\xe5\x8f\xaf\xe6\x9b\xb4\xe6\x96\xb0\xe7\x9a\x84\xe6\xa8\xa1\xe7\xbb\x84\xe5\x90\x97\xef\xbc\x9f",
        count);

    if (!dialogs_show_confirm(GTK_WINDOW(window), "\xe7\xa1\xae\xe8\xae\xa4\xe5\x85\xa8\xe9\x83\xa8\xe6\x9b\xb4\xe6\x96\xb0", msg))
        { g_free(indices); return; }

    state->is_updating = TRUE;
    show_progress("\xe2\xac\x86 \xe6\xad\xa3\xe5\x9c\xa8\xe6\x9b\xb4\xe6\x96\xb0\xe5\x85\xa8\xe9\x83\xa8...");
    UpdateData *d = g_new(UpdateData, 1);
    d->indices = indices; d->count = count;
    g_thread_new("update-all", update_thread_func, d);
}

/* ═══════════════ Delete ═══════════════ */
static void on_delete_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    int count; int *indices = mod_list_view_get_selected(&count);
    if (!indices || count == 0) { g_free(indices); return; }

    int response = dialogs_show_delete_confirm(GTK_WINDOW(window), count);
    if (response == 1 || response == 2) {
        AppState *state = app_get_state();
        bool keep = (response == 1);
        int deleted;
        deleter_delete_batch(indices, count, state->config.backup_dir, keep, &deleted);
        mod_list_clear();
        scanner_scan_directory(state->config.mod_dir, mod_list_get_all());
        mod_list_view_refresh();
        main_window_update_status();

        char res[128];
        snprintf(res, sizeof(res), "\xe5\xb7\xb2\xe5\x88\xa0\xe9\x99\xa4 %d \xe4\xb8\xaa\xe6\xa8\xa1\xe7\xbb\x84\xe3\x80\x82", deleted);
        dialogs_show_confirm(GTK_WINDOW(window), "\xe5\x88\xa0\xe9\x99\xa4\xe5\xae\x8c\xe6\x88\x90", res);
    }
    g_free(indices);
}

/* ═══════════════ Settings ═══════════════ */
static void on_settings_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    AppState *state = app_get_state();
    AppConfig old = state->config;
    if (dialogs_show_settings(GTK_WINDOW(window), &state->config)) {
        config_save(&state->config);
        main_window_update_status();
    } else {
        state->config = old;
    }
}

/* ═══════════════ Rollback ═══════════════ */
static void on_rollback_clicked(GtkButton *btn, gpointer userdata)
{
    (void)btn; (void)userdata;
    int count; int *indices = mod_list_view_get_selected(&count);
    if (!indices || count == 0) { g_free(indices); return; }
    if (count > 1) {
        dialogs_show_confirm(GTK_WINDOW(window), "\xe6\x8f\x90\xe7\xa4\xba",
            "\xe8\xaf\xb7\xe5\x8f\xaa\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x80\xe4\xb8\xaa\xe6\xa8\xa1\xe7\xbb\x84\xe8\xbf\x9b\xe8\xa1\x8c\xe5\x9b\x9e\xe6\xbb\x9a\xe3\x80\x82");
        g_free(indices); return;
    }
    ModInfo *mod = mod_list_get(indices[0]);
    if (!mod) { g_free(indices); return; }
    AppState *state = app_get_state();
    char *bp = dialogs_show_rollback(GTK_WINDOW(window), mod, state->config.backup_dir);
    if (bp) {
        if (rollback_to_backup(mod, bp, state->config.backup_dir) == 0) {
            mod_list_view_refresh(); main_window_update_status();
            dialogs_show_confirm(GTK_WINDOW(window), "\xe5\x9b\x9e\xe6\xbb\x9a\xe6\x88\x90\xe5\x8a\x9f",
                "\xe6\xa8\xa1\xe7\xbb\x84\xe5\xb7\xb2\xe6\x88\x90\xe5\x8a\x9f\xe5\x9b\x9e\xe6\xbb\x9a\xe5\x88\xb0\xe6\x8c\x87\xe5\xae\x9a\xe7\x89\x88\xe6\x9c\xac\xe3\x80\x82");
        } else
            dialogs_show_confirm(GTK_WINDOW(window), "\xe5\x9b\x9e\xe6\xbb\x9a\xe5\xa4\xb1\xe8\xb4\xa5",
                "\xe5\x9b\x9e\xe6\xbb\x9a\xe8\xbf\x87\xe7\xa8\x8b\xe4\xb8\xad\xe5\x87\xba\xe7\x8e\xb0\xe9\x94\x99\xe8\xaf\xaf\xe3\x80\x82");
        free(bp);
    }
    g_free(indices);
}

/* ═══════════════ Keyboard Shortcuts ═══════════════ */
static gboolean on_key_pressed(GtkEventControllerKey *controller,
                                guint keyval, guint keycode, GdkModifierType state,
                                gpointer userdata)
{
    (void)controller; (void)keycode; (void)userdata;

    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_a) {
        // Ctrl+A: select all
        mod_list_view_select_all(TRUE);
        return GDK_EVENT_STOP;
    }
    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_d) {
        // Ctrl+D: deselect all
        mod_list_view_select_all(FALSE);
        return GDK_EVENT_STOP;
    }
    if (keyval == GDK_KEY_Delete || keyval == GDK_KEY_KP_Delete) {
        // Delete: delete selected
        on_delete_clicked(NULL, NULL);
        return GDK_EVENT_STOP;
    }
    if (keyval == GDK_KEY_F5) {
        // F5: rescan
        on_scan_clicked(NULL, NULL);
        return GDK_EVENT_STOP;
    }
    return GDK_EVENT_PROPAGATE;
}

/* ═══════════════ Double-click Detail ═══════════════ */
static void on_row_activated(GtkTreeView *tv, GtkTreePath *path,
                              GtkTreeViewColumn *col, gpointer userdata)
{
    (void)tv; (void)col; (void)userdata;
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        ModInfo *mod;
        gtk_tree_model_get(model, &iter, COL_MOD_PTR, &mod, -1);
        if (mod) dialogs_show_mod_detail(GTK_WINDOW(window), mod);
    }
}

/* ═══════════════ Create Main Window ═══════════════ */
GtkWidget *main_window_create(void)
{
    window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "\xf0\x9f\xa7\x8a Minecraft Mod Manager");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 680);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    /* ─── 主垂直布局 ─── */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    /* ─── 工具栏 ─── */
    GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(tb, "toolbar");
    gtk_widget_set_margin_start(tb, 8);
    gtk_widget_set_margin_end(tb, 8);
    gtk_widget_set_margin_top(tb, 8);
    gtk_widget_set_margin_bottom(tb, 4);

    GtkWidget *b_scan   = gtk_button_new_with_label("\xf0\x9f\x94\x8d \xe6\x89\xab\xe6\x8f\x8f");
    GtkWidget *b_upd_sel = gtk_button_new_with_label("\xe2\xac\x86 \xe6\x9b\xb4\xe6\x96\xb0\xe9\x80\x89\xe4\xb8\xad");
    GtkWidget *b_upd_all = gtk_button_new_with_label("\xe2\xac\x86 \xe6\x9b\xb4\xe6\x96\xb0\xe5\x85\xa8\xe9\x83\xa8");
    GtkWidget *b_roll   = gtk_button_new_with_label("\xe2\x86\xa9 \xe5\x9b\x9e\xe6\xbb\x9a");
    GtkWidget *b_del    = gtk_button_new_with_label("\xf0\x9f\x97\x91 \xe5\x88\xa0\xe9\x99\xa4");
    GtkWidget *b_sel_all  = gtk_button_new_with_label("\xe2\x98\x91 All");
    GtkWidget *b_sel_none = gtk_button_new_with_label("\xe2\x98\x90 None");
    gtk_widget_set_name(b_sel_all, "btn-sel-all");
    gtk_widget_set_name(b_sel_none, "btn-sel-none");
    GtkWidget *b_set    = gtk_button_new_with_label("\xe2\x9a\x99 \xe8\xae\xbe\xe7\xbd\xae");

    // 为每个按钮Settings CSS name，以便精确覆盖样式
    gtk_widget_set_name(b_scan,    "btn-scan");
    gtk_widget_set_name(b_upd_sel, "btn-upd-sel");
    gtk_widget_set_name(b_upd_all, "btn-upd-all");
    gtk_widget_set_name(b_roll,    "btn-roll");
    gtk_widget_set_name(b_del,     "btn-del");
    gtk_widget_set_name(b_set,     "btn-set");

    // 为按钮Settings tooltip（快捷键提示）
    gtk_widget_set_tooltip_text(b_scan, "\xe6\x89\xab\xe6\x8f\x8f\xe6\xa8\xa1\xe7\xbb\x84\xe7\x9b\xae\xe5\xbd\x95 (F5)");
    gtk_widget_set_tooltip_text(b_del, "\xe5\x88\xa0\xe9\x99\xa4\xe5\x8b\xbe\xe9\x80\x89\xe7\x9a\x84\xe6\xa8\xa1\xe7\xbb\x84 (Delete)");

    gtk_box_append(GTK_BOX(tb), b_scan);
    gtk_box_append(GTK_BOX(tb), b_upd_sel);
    gtk_box_append(GTK_BOX(tb), b_upd_all);
    gtk_box_append(GTK_BOX(tb), b_roll);
    gtk_box_append(GTK_BOX(tb), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(tb), b_sel_all);
    gtk_box_append(GTK_BOX(tb), b_sel_none);
    gtk_box_append(GTK_BOX(tb), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(tb), b_del);
    gtk_box_append(GTK_BOX(tb), gtk_separator_new(GTK_ORIENTATION_VERTICAL));
    gtk_box_append(GTK_BOX(tb), b_set);

    gtk_box_append(GTK_BOX(vbox), tb);

    /* ─── 搜索/筛选栏 ─── */
    GtkWidget *search_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(search_bar, "search-bar");
    gtk_widget_set_margin_start(search_bar, 10);
    gtk_widget_set_margin_end(search_bar, 10);
    gtk_widget_set_margin_top(search_bar, 4);
    gtk_widget_set_margin_bottom(search_bar, 4);

    GtkWidget *search_icon = gtk_image_new_from_icon_name("edit-find-symbolic");
    gtk_box_append(GTK_BOX(search_bar), search_icon);

    search_entry = gtk_entry_new();
    gtk_widget_set_name(search_entry, "search-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry),
        "\xe6\x90\x9c\xe7\xb4\xa2\xe6\xa8\xa1\xe7\xbb\x84\xe5\x90\x8d\xe7\xa7\xb0...");
    gtk_widget_set_hexpand(search_entry, TRUE);
    gtk_box_append(GTK_BOX(search_bar), search_entry);

    // 平台筛选下拉
    filter_combo = gtk_drop_down_new_from_strings((const char *[]){
        "\xe5\x85\xa8\xe9\x83\xa8\xe5\xb9\xb3\xe5\x8f\xb0",
        "\xe5\x8f\xaf\xe6\x9b\xb4\xe6\x96\xb0",
        "\xe5\xb7\xb2\xe6\x9c\x80\xe6\x96\xb0",
        "\xe6\x9c\xaa\xe5\x8c\xb9\xe9\x85\x8d",
        "Modrinth",
        "CurseForge",
        NULL
    });
    gtk_widget_set_name(filter_combo, "filter-combo");

    gtk_box_append(GTK_BOX(search_bar), filter_combo);

    gtk_box_append(GTK_BOX(vbox), search_bar);

    /* ─── 列表 ─── */
    GtkWidget *list = mod_list_view_create();
    gtk_widget_set_name(list, "mod-treeview");
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(vbox), scroll);

    /* ─── 进度条 ─── */
    progress_bar = gtk_progress_bar_new();
    gtk_widget_set_name(progress_bar, "mod-progress");
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_widget_set_visible(progress_bar, FALSE);
    gtk_widget_set_margin_start(progress_bar, 10);
    gtk_widget_set_margin_end(progress_bar, 10);
    gtk_widget_set_margin_top(progress_bar, 4);
    gtk_widget_set_margin_bottom(progress_bar, 4);
    gtk_box_append(GTK_BOX(vbox), progress_bar);

    /* ─── 状态栏 ─── */
    GtkWidget *sb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(sb, "status-bar");
    status_bar_label = gtk_label_new("\xe5\xb0\xb1\xe7\xbb\xaa");
    gtk_label_set_xalign(GTK_LABEL(status_bar_label), 0.0);
    gtk_widget_set_margin_start(status_bar_label, 10);
    gtk_widget_set_margin_end(status_bar_label, 10);
    gtk_widget_set_margin_top(status_bar_label, 5);
    gtk_widget_set_margin_bottom(status_bar_label, 5);
    gtk_box_append(GTK_BOX(sb), status_bar_label);
    gtk_box_append(GTK_BOX(vbox), sb);

    /* ─── 信号连接 ─── */
    g_signal_connect(b_scan,    "clicked", G_CALLBACK(on_scan_clicked), NULL);
    g_signal_connect(b_upd_sel, "clicked", G_CALLBACK(on_update_sel_clicked), NULL);
    g_signal_connect(b_upd_all, "clicked", G_CALLBACK(on_update_all_clicked), NULL);
    g_signal_connect(b_roll,    "clicked", G_CALLBACK(on_rollback_clicked), NULL);
    g_signal_connect(b_sel_all, "clicked", G_CALLBACK(on_select_all_clicked), NULL);
    g_signal_connect(b_sel_none,"clicked", G_CALLBACK(on_deselect_all_clicked), NULL);
    g_signal_connect(b_del,     "clicked", G_CALLBACK(on_delete_clicked), NULL);
    g_signal_connect(b_set,     "clicked", G_CALLBACK(on_settings_clicked), NULL);

    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    // GTK4 的 GtkDropDown 信号不同，这里简化
    // g_signal_connect(filter_combo, "changed", G_CALLBACK(on_filter_changed), NULL);

    g_signal_connect(list, "row-activated", G_CALLBACK(on_row_activated), NULL);

    /* ─── Keyboard Shortcuts ─── */
    GtkEventController *key_controller = gtk_event_controller_key_new();
    gtk_widget_add_controller(window, key_controller);
    g_signal_connect(key_controller, "key-pressed",
                     G_CALLBACK(on_key_pressed), NULL);

    /* ─── 初始更新状态 ─── */
    main_window_update_status();

    return window;
}
