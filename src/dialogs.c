#include "dialogs.h"
#include "rollback.h"
#include <string.h>
#include <stdlib.h>
#include <windows.h>

/* ═══════════════════════════════════════════
 * 对话框辅助：GTK4 中没有 gtk_dialog_run，
 * 我们用 Win32 MessageBox 做简单对话框，
 * 用 GTK 窗口自己做复杂对话框。
 * ═══════════════════════════════════════════ */

/* ─── 确认对话框（Win32 MessageBox） ─── */
gboolean dialogs_show_confirm(GtkWindow *parent, const char *title, const char *message)
{
    (void)parent;
    int ret = MessageBoxA(NULL, message, title,
        MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST);
    return (ret == IDOK);
}

/* ─── 删除确认对话框（Win32 MessageBox） ─── */
int dialogs_show_delete_confirm(GtkWindow *parent, int count)
{
    (void)parent;
    char msg[512];
    snprintf(msg, sizeof(msg),
        "确定要删除选中的 %d 个模组吗？\n\n"
        "按「是」= 删除并保留备份\n"
        "按「否」= 直接删除\n"
        "按「取消」= 取消操作", count);

    int ret = MessageBoxA(NULL, msg, "确认删除",
        MB_YESNOCANCEL | MB_ICONWARNING | MB_TOPMOST);

    switch (ret) {
        case IDYES:    return 1;  // 保留备份删除
        case IDNO:     return 2;  // 直接删除
        default:       return 0;  // 取消
    }
}

/* ─── 更新结果对话框 ─── */
void dialogs_show_update_result(GtkWindow *parent, int success, int failed,
                                 const char errors[10][512], int error_count)
{
    (void)parent;
    char summary[2048];
    int n = snprintf(summary, sizeof(summary),
        "更新完成!\n\n成功: %d  失败: %d", success, failed);

    if (error_count > 0) {
        n += snprintf(summary + n, sizeof(summary) - n, "\n\n错误详情:");
        for (int i = 0; i < error_count && i < 5; i++) {
            n += snprintf(summary + n, sizeof(summary) - n, "\n• %s", errors[i]);
        }
        if (error_count > 5)
            snprintf(summary + n, sizeof(summary) - n, "\n...及其他 %d 个错误", error_count - 5);
    }

    MessageBoxA(NULL, summary, "更新结果", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}

/* ─── 辅助：给 GtkBox 添加带标签的行 ─── */
static GtkWidget *add_labeled_row(GtkWidget *box, const char *label_text, GtkWidget *widget)
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_widget_set_size_request(label, 130, -1);
    gtk_box_append(GTK_BOX(hbox), label);
    gtk_box_append(GTK_BOX(hbox), widget);
    gtk_box_append(GTK_BOX(box), hbox);
    return hbox;
}

/* ─── 保存设置回调 ─── */
static void on_settings_save(GtkButton *btn, GtkWindow *dialog)
{
    (void)btn;
    AppConfig *config = (AppConfig *)g_object_get_data(G_OBJECT(dialog), "config");
    if (!config) return;

    GtkWidget *mod_dir_e = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "mod-dir-entry"));
    GtkWidget *bak_dir_e = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "bak-dir-entry"));
    GtkWidget *max_bak_s = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "max-bak-spin"));
    GtkWidget *cf_e = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "cf-entry"));
    GtkWidget *auto_s = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "auto-scan-switch"));
    GtkWidget *backup_s = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "backup-switch"));

    if (mod_dir_e) snprintf(config->mod_dir, sizeof(config->mod_dir), "%s",
        gtk_editable_get_text(GTK_EDITABLE(mod_dir_e)));
    if (bak_dir_e) snprintf(config->backup_dir, sizeof(config->backup_dir), "%s",
        gtk_editable_get_text(GTK_EDITABLE(bak_dir_e)));
    if (max_bak_s) config->max_backups = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(max_bak_s));
    if (cf_e) snprintf(config->curseforge_api_key, sizeof(config->curseforge_api_key), "%s",
        gtk_editable_get_text(GTK_EDITABLE(cf_e)));
    if (auto_s) config->auto_scan_on_start = gtk_switch_get_active(GTK_SWITCH(auto_s));
    if (backup_s) config->backup_before_update = gtk_switch_get_active(GTK_SWITCH(backup_s));

    config_save(config);
    gtk_window_destroy(dialog);
}

/* ─── 设置对话框 ─── */
int dialogs_show_settings(GtkWindow *parent, AppConfig *config)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "设置 - Minecraft Mod Manager");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 350);
    gtk_widget_set_name(dialog, "settings-dialog");

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(content_box, 12);
    gtk_widget_set_margin_end(content_box, 12);
    gtk_widget_set_margin_top(content_box, 12);
    gtk_widget_set_margin_bottom(content_box, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), content_box);

    /* 创建输入控件 */
    GtkWidget *mod_dir_e = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(mod_dir_e), config->mod_dir);
    gtk_widget_set_hexpand(mod_dir_e, TRUE);
    gtk_widget_set_name(mod_dir_e, "settings-entry");
    GtkWidget *r1 = add_labeled_row(content_box, "模组目录:", mod_dir_e);

    GtkWidget *bak_dir_e = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(bak_dir_e), config->backup_dir);
    gtk_widget_set_hexpand(bak_dir_e, TRUE);
    gtk_widget_set_name(bak_dir_e, "settings-entry");
    GtkWidget *r2 = add_labeled_row(content_box, "备份目录:", bak_dir_e);

    GtkWidget *max_bak_s = gtk_spin_button_new_with_range(0, 99, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_bak_s), config->max_backups);
    gtk_widget_set_name(max_bak_s, "settings-spin");
    GtkWidget *r3 = add_labeled_row(content_box, "最大备份数:", max_bak_s);

    GtkWidget *cf_e = gtk_password_entry_new();
    if (config->curseforge_api_key[0])
        gtk_editable_set_text(GTK_EDITABLE(cf_e), config->curseforge_api_key);
    gtk_widget_set_hexpand(cf_e, TRUE);
    gtk_widget_set_name(cf_e, "settings-password");
    GtkWidget *r4 = add_labeled_row(content_box, "CurseForge API Key:", cf_e);

    GtkWidget *auto_s = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(auto_s), config->auto_scan_on_start);
    gtk_widget_set_name(auto_s, "settings-switch");
    GtkWidget *r5 = add_labeled_row(content_box, "启动时自动扫描:", auto_s);

    GtkWidget *backup_s = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(backup_s), config->backup_before_update);
    gtk_widget_set_name(backup_s, "settings-switch");
    GtkWidget *r6 = add_labeled_row(content_box, "更新前自动备份:", backup_s);

    /* 存储控件 */
    g_object_set_data(G_OBJECT(dialog), "config", config);
    g_object_set_data(G_OBJECT(dialog), "mod-dir-entry", mod_dir_e);
    g_object_set_data(G_OBJECT(dialog), "bak-dir-entry", bak_dir_e);
    g_object_set_data(G_OBJECT(dialog), "max-bak-spin", max_bak_s);
    g_object_set_data(G_OBJECT(dialog), "cf-entry", cf_e);
    g_object_set_data(G_OBJECT(dialog), "auto-scan-switch", auto_s);
    g_object_set_data(G_OBJECT(dialog), "backup-switch", backup_s);

    /* 按钮 */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);
    GtkWidget *btn_cancel = gtk_button_new_with_label("取消");
    GtkWidget *btn_save = gtk_button_new_with_label("保存");
    gtk_widget_set_name(btn_cancel, "settings-btn-cancel");
    gtk_widget_set_name(btn_save, "settings-btn-save");
    gtk_box_append(GTK_BOX(btn_box), btn_cancel);
    gtk_box_append(GTK_BOX(btn_box), btn_save);
    gtk_box_append(GTK_BOX(content_box), btn_box);

    g_signal_connect_swapped(btn_cancel, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_settings_save), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
    return 1; /* 非阻塞，总是返回成功 */
}

/* ─── 模组详情对话框 ─── */
void dialogs_show_mod_detail(GtkWindow *parent, const ModInfo *mod)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "模组详情");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 400);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    const char *source_str = mod->source == MOD_SOURCE_CURSEFORGE ? "CurseForge" :
                             mod->source == MOD_SOURCE_MODRINTH ? "Modrinth" : "未知";
    const char *status_str = mod->status == MOD_STATUS_UP_TO_DATE ? "✅ 最新" :
                             mod->status == MOD_STATUS_UPDATE_AVAIL ? "⬆ 可更新" :
                             mod->status == MOD_STATUS_UNKNOWN ? "⚠ 未匹配" : "❌ 错误";

    char size_buf[32];
    if (mod->file_size > 1024*1024)
        snprintf(size_buf, sizeof(size_buf), "%.1f MB", mod->file_size/(1024.0*1024.0));
    else if (mod->file_size > 1024)
        snprintf(size_buf, sizeof(size_buf), "%.0f KB", mod->file_size/1024.0);
    else
        snprintf(size_buf, sizeof(size_buf), "%ld B", mod->file_size);

    const char *fields[] = {"模组名称:", mod->name, "模组ID:", mod->mod_id,
        "本地版本:", mod->local_version, "最新版本:", mod->latest_version[0]?mod->latest_version:"-",
        "MC版本:", mod->mc_version[0]?mod->mc_version:"-",
        "加载器:", mod->loader[0]?mod->loader:"-",
        "来源平台:", source_str, "状态:", status_str,
        "文件大小:", size_buf, "文件名:", mod->file_name, NULL};

    for (int i = 0; fields[i] != NULL; i += 2) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *l = gtk_label_new(fields[i]);
        gtk_label_set_xalign(GTK_LABEL(l), 1.0);
        gtk_widget_set_size_request(l, 100, -1);
        GtkWidget *v = gtk_label_new(fields[i+1]);
        gtk_label_set_xalign(GTK_LABEL(v), 0.0);
        gtk_label_set_selectable(GTK_LABEL(v), TRUE);
        gtk_box_append(GTK_BOX(row), l);
        gtk_box_append(GTK_BOX(row), v);
        gtk_box_append(GTK_BOX(box), row);
    }

    GtkWidget *close_btn = gtk_button_new_with_label("关闭");
    gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
    gtk_widget_set_margin_top(close_btn, 12);
    gtk_box_append(GTK_BOX(box), close_btn);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ─── 回滚对话框 ─── */
char *dialogs_show_rollback(GtkWindow *parent, const ModInfo *mod, const char *backup_dir)
{
    (void)parent;
    char title[256];
    snprintf(title, sizeof(title), "回滚 - %s (当前: %s)", mod->name, mod->local_version);

    // 列出备份
    BackupList blist;
    memset(&blist, 0, sizeof(blist));
    rollback_list_backups(backup_dir, mod->name, &blist);

    if (blist.count == 0) {
        MessageBoxA(NULL, "没有可用的备份版本。", title, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        return NULL;
    }

    // ─── MessageBox + 编号选择 ───
    // 因 GTK4 缺少阻塞模态对话框，用 Win32 方案
    
    // 构造版本列表消息（最多显示9个）
    char msg[2048] = "选择要回滚的版本:\n\n";
    int show_count = blist.count > 9 ? 9 : blist.count;
    for (int i = 0; i < show_count; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%d. %s  (%s)\n",
            i + 1,
            blist.entries[blist.count - 1 - i].version,
            blist.entries[blist.count - 1 - i].timestamp);
        strcat(msg, line);
    }
    if (blist.count > 9) {
        char more[64];
        snprintf(more, sizeof(more), "\n...及其他 %d 个版本", blist.count - 9);
        strcat(msg, more);
    }

    char prompt[1024];
    snprintf(prompt, sizeof(prompt), "请输入版本编号 (1-%d):", show_count);

    // 用 VBScript 弹出 InputBox
    char vbscript[4096];
    snprintf(vbscript, sizeof(vbscript),
        "mshta \"javascript:var n=prompt('%s','1');"
        "if(n!=null){var s=String(n);"
        "var fso=new ActiveXObject('Scripting.FileSystemObject');"
        "var wsh=fso.CreateTextFile('%s\\\\\\\\modmgr_sel.txt',true);"
        "wsh.Write(s);wsh.Close()}close()\"",
        prompt, backup_dir);

    // 先显示 MessageBox 告知用户接下来会弹出输入框
    MessageBoxA(NULL, msg, title, MB_OK | MB_ICONINFORMATION | MB_TOPMOST);

    // 弹出输入框
    system(vbscript);

    // 读取选择
    char sel_path[MAX_PATH];
    snprintf(sel_path, sizeof(sel_path), "%s\\modmgr_sel.txt", backup_dir);
    FILE *sf = fopen(sel_path, "r");
    int choice = 1;
    if (sf) {
        if (fscanf(sf, "%d", &choice) != 1) choice = 1;
        fclose(sf);
        remove(sel_path);
    }

    if (choice < 1 || choice > show_count) choice = 1;

    int idx = blist.count - choice; // 倒序（列表中先显示最新）
    if (idx < 0) idx = 0;

    char *path = malloc(2048);
    if (path) {
        snprintf(path, 2048, "%s\\%s", backup_dir, blist.entries[idx].file_name);
    }
    return path;
}

/* ─── 进度对话框 ─── */
GtkDialog *dialogs_show_progress(GtkWindow *parent, const char *title)
{
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 120);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget *label = gtk_label_new("正在处理...");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), TRUE);
    gtk_box_append(GTK_BOX(box), bar);

    g_object_set_data(G_OBJECT(dialog), "status-label", label);
    g_object_set_data(G_OBJECT(dialog), "progress-bar", bar);

    gtk_window_present(GTK_WINDOW(dialog));

    return GTK_DIALOG(dialog);
}

void dialogs_update_progress(GtkDialog *dialog, const char *text, double fraction)
{
    GtkWidget *label = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "status-label"));
    GtkWidget *bar = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "progress-bar"));

    if (label) gtk_label_set_text(GTK_LABEL(label), text);
    if (bar) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), fraction);
}

void dialogs_close_progress(GtkDialog *dialog)
{
    if (dialog && GTK_IS_WINDOW(dialog))
        gtk_window_destroy(GTK_WINDOW(dialog));
}
