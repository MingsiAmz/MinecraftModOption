#ifndef DIALOGS_H
#define DIALOGS_H

#include <gtk/gtk.h>
#include "mod_list.h"
#include "config.h"

/**
 * 显示设置对话框
 * @param parent 父窗口
 * @param config 配置（可修改）
 * @return 0取消，1保存
 */
int dialogs_show_settings(GtkWindow *parent, AppConfig *config);

/**
 * 显示模组详情对话框
 * @param parent 父窗口
 * @param mod    模组信息
 */
void dialogs_show_mod_detail(GtkWindow *parent, const ModInfo *mod);

/**
 * 显示回滚对话框，选择要回滚的版本
 * @param parent     父窗口
 * @param mod        模组信息
 * @param backup_dir 备份目录
 * @return 选择的备份文件路径（需 free），NULL 表示取消
 */
char *dialogs_show_rollback(GtkWindow *parent, const ModInfo *mod, const char *backup_dir);

/**
 * 显示确认对话框
 * @param parent  父窗口
 * @param title   标题
 * @param message 消息
 * @return TRUE 确认，FALSE 取消
 */
gboolean dialogs_show_confirm(GtkWindow *parent, const char *title, const char *message);

/**
 * 显示进度对话框（用于后台操作）
 * @param parent  父窗口
 * @param title   标题
 * @return 进度对话框指针（用于更新）
 */
GtkDialog *dialogs_show_progress(GtkWindow *parent, const char *title);

/**
 * 更新进度对话框
 * @param dialog  进度对话框
 * @param text    当前状态文本
 * @param fraction 进度 0.0 ~ 1.0
 */
void dialogs_update_progress(GtkDialog *dialog, const char *text, double fraction);

/**
 * 关闭进度对话框
 */
void dialogs_close_progress(GtkDialog *dialog);

/**
 * 显示删除确认对话框（支持保留备份/直接删除/取消）
 * @param parent 父窗口
 * @param count  要删除的数量
 * @return 1=保留备份删除, 2=直接删除, 0=取消
 */
int dialogs_show_delete_confirm(GtkWindow *parent, int count);

/**
 * 显示更新结果对话框
 */
void dialogs_show_update_result(GtkWindow *parent, int success, int failed, const char errors[10][512], int error_count);

#endif /* DIALOGS_H */
