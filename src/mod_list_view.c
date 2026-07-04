#include "mod_list_view.h"
#include <string.h>
#include <stdlib.h>

/* 
 * 使用 GtkTreeView（GTK4 中 deprecated 但可用）
 * 用普通回调函数，不用 lambda
 */

static GtkWidget *tree_view = NULL;
static GtkListStore *store = NULL;

static const char *status_text(int status)
{
    switch (status) {
        case MOD_STATUS_UP_TO_DATE:   return "\xe2\x9c\x85 \xe6\x9c\x80\xe6\x96\xb0";
        case MOD_STATUS_UPDATE_AVAIL: return "\xe2\xac\x86 \xe5\x8f\xaf\xe6\x9b\xb4\xe6\x96\xb0";
        case MOD_STATUS_UNKNOWN:      return "\xe2\x9a\xa0 \xe6\x9c\xaa\xe5\x8c\xb9\xe9\x85\x8d";
        case MOD_STATUS_ERROR:        return "\xe2\x9d\x8c \xe9\x94\x99\xe8\xaf\xaf";
        default: return "\xe6\x9c\xaa\xe7\x9f\xa5";
    }
}

static const char *source_text(int source)
{
    switch (source) {
        case MOD_SOURCE_CURSEFORGE: return "CurseForge";
        case MOD_SOURCE_MODRINTH:   return "Modrinth";
        default: return "\xe6\x9c\xaa\xe7\x9f\xa5";
    }
}

static void format_size(long bytes, char *buf, size_t buf_size)
{
    if (bytes > 1024 * 1024)
        snprintf(buf, buf_size, "%.1f MB", bytes / (1024.0 * 1024.0));
    else if (bytes > 1024)
        snprintf(buf, buf_size, "%.0f KB", bytes / 1024.0);
    else
        snprintf(buf, buf_size, "%ld B", bytes);
}

/* ─── 复选框回调 ─── */
static void on_toggle_checked(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
    (void)cell; (void)data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
        gboolean val;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_CHECK, &val, -1);
        gtk_list_store_set(store, &iter, COL_CHECK, !val, -1);
    }
    gtk_tree_path_free(path);
}

/* ─── 创建视图 ─── */
GtkWidget *mod_list_view_create(void)
{
    GType types[] = {
        G_TYPE_BOOLEAN,    // COL_CHECK
        G_TYPE_STRING,     // COL_NAME
        G_TYPE_STRING,     // COL_LOCAL_VER
        G_TYPE_STRING,     // COL_LATEST_VER
        G_TYPE_STRING,     // COL_SOURCE
        G_TYPE_STRING,     // COL_STATUS
        G_TYPE_STRING,     // COL_SIZE
        G_TYPE_POINTER     // COL_MOD_PTR
    };

    store = gtk_list_store_newv(NUM_COLS, types);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    const char *titles[] = {"", "\xe6\xa8\xa1\xe7\xbb\x84\xe5\x90\x8d\xe7\xa7\xb0",
                            "\xe6\x9c\xac\xe5\x9c\xb0\xe7\x89\x88\xe6\x9c\xac",
                            "\xe6\x9c\x80\xe6\x96\xb0\xe7\x89\x88\xe6\x9c\xac",
                            "\xe6\x9d\xa5\xe6\xba\x90", "\xe7\x8a\xb6\xe6\x80\x81", "\xe5\xa4\xa7\xe5\xb0\x8f"};
    int cols[] = {COL_CHECK, COL_NAME, COL_LOCAL_VER, COL_LATEST_VER,
                  COL_SOURCE, COL_STATUS, COL_SIZE};
    int widths[] = {30, 200, 100, 100, 100, 90, 80};

    for (int i = 0; i < 7; i++) {
        GtkCellRenderer *renderer;
        if (i == 0) {
            renderer = gtk_cell_renderer_toggle_new();
            g_signal_connect(renderer, "toggled", G_CALLBACK(on_toggle_checked), NULL);
        } else {
            renderer = gtk_cell_renderer_text_new();
        }

        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
            titles[i], renderer, i == 0 ? "active" : "text", cols[i], NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        if (i > 0)
            gtk_tree_view_column_set_sort_column_id(column, cols[i]);
        gtk_tree_view_column_set_fixed_width(column, widths[i]);

        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    }

    gtk_tree_selection_set_mode(
        gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view)),
        GTK_SELECTION_MULTIPLE);

    gtk_widget_set_hexpand(tree_view, TRUE);
    gtk_widget_set_vexpand(tree_view, TRUE);

    return tree_view;
}

void mod_list_view_refresh(void)
{
    if (!store) return;
    gtk_list_store_clear(store);

    GPtrArray *mods = mod_list_get_all();
    if (!mods) return;

    for (int i = 0; i < (int)mods->len; i++) {
        ModInfo *mod = mod_list_get(i);
        if (!mod) continue;

        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);

        char size_str[32];
        format_size(mod->file_size, size_str, sizeof(size_str));

        gtk_list_store_set(store, &iter,
            COL_CHECK, FALSE,
            COL_NAME, mod->name,
            COL_LOCAL_VER, mod->local_version,
            COL_LATEST_VER, mod->latest_version[0] ? mod->latest_version : "-",
            COL_SOURCE, source_text(mod->source),
            COL_STATUS, status_text(mod->status),
            COL_SIZE, size_str,
            COL_MOD_PTR, mod,
            -1);
    }
}

int *mod_list_view_get_selected(int *count)
{
    *count = 0;
    if (!tree_view) return NULL;

    /* 改为获取勾选的 (COL_CHECK == TRUE) 行 */
    int total = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
    int *indices = g_new(int, total);
    int idx = 0;

    for (int i = 0; i < total; i++) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(i, -1);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
            gboolean checked;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_CHECK, &checked, -1);
            if (checked) {
                indices[idx++] = i;
            }
        }
        gtk_tree_path_free(path);
    }

    *count = idx;
    if (idx == 0) {
        g_free(indices);
        return NULL;
    }
    return indices;
}

void mod_list_view_select_all(gboolean select)
{
    if (!store) return;
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gtk_list_store_set(store, &iter, COL_CHECK, select, -1);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
}

/* ─── 搜索/筛选 ─── */
void mod_list_view_set_filter(const char *search_text, const char *filter)
{
    if (!store) return;

    // 在 GTK4 中 GtkTreeView 没有内置的行隐藏
    // 简单实现：重新填充列表，跳过不匹配的条目
    // 注：更好的方式是用 GtkFilterListModel，但为了保持简单
    // 我们重新从 mod_list 填充，但记住勾选状态

    // 保存当前勾选状态
    int total = mod_list_count();
    int *checked = g_new0(int, total);
    for (int i = 0; i < total; i++) {
        GtkTreePath *path = gtk_tree_path_new_from_indices(i, -1);
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) {
            gboolean val;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_CHECK, &val, -1);
            checked[i] = val ? 1 : 0;
        }
        gtk_tree_path_free(path);
    }

    gtk_list_store_clear(store);
    GPtrArray *mods = mod_list_get_all();
    if (!mods) { g_free(checked); return; }

    gboolean search_active = (search_text && search_text[0]);
    gboolean filter_active = (filter && filter[0]);

    for (int i = 0; i < (int)mods->len; i++) {
        ModInfo *mod = mod_list_get(i);
        if (!mod) continue;

        // 搜索过滤
        if (search_active) {
            char search_lower[256], name_lower[256];
            snprintf(search_lower, sizeof(search_lower), "%s", search_text);
            for (char *p = search_lower; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            snprintf(name_lower, sizeof(name_lower), "%s", mod->name);
            for (char *p = name_lower; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            if (!strstr(name_lower, search_lower)) continue;
        }

        // 筛选过滤
        if (filter_active) {
            if (strcmp(filter, "\xe5\x8f\xaf\xe6\x9b\xb4\xe6\x96\xb0") == 0) {
                if (mod->status != MOD_STATUS_UPDATE_AVAIL) continue;
            } else if (strcmp(filter, "\xe5\xb7\xb2\xe6\x9c\x80\xe6\x96\xb0") == 0) {
                if (mod->status != MOD_STATUS_UP_TO_DATE) continue;
            } else if (strcmp(filter, "\xe6\x9c\xaa\xe5\x8c\xb9\xe9\x85\x8d") == 0) {
                if (mod->status != MOD_STATUS_UNKNOWN) continue;
            } else if (strcmp(filter, "Modrinth") == 0) {
                if (mod->source != MOD_SOURCE_MODRINTH) continue;
            } else if (strcmp(filter, "CurseForge") == 0) {
                if (mod->source != MOD_SOURCE_CURSEFORGE) continue;
            }
        }

        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);

        char size_str[32];
        format_size(mod->file_size, size_str, sizeof(size_str));

        gtk_list_store_set(store, &iter,
            COL_CHECK, checked[i] ? TRUE : FALSE,
            COL_NAME, mod->name,
            COL_LOCAL_VER, mod->local_version,
            COL_LATEST_VER, mod->latest_version[0] ? mod->latest_version : "-",
            COL_SOURCE, source_text(mod->source),
            COL_STATUS, status_text(mod->status),
            COL_SIZE, size_str,
            COL_MOD_PTR, mod,
            -1);
    }

    g_free(checked);
}
