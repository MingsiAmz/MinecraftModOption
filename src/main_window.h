#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>

/**
 * 创建并显示主窗口
 */
GtkWidget *main_window_create(void);

/**
 * 更新状态栏信息
 */
void main_window_update_status(void);

/**
 * 启动一次模组扫描（解析 jar + SHA1 识别 + 更新检测）
 * 已有扫描在进行时直接返回。
 */
void main_window_start_scan(void);

#endif /* MAIN_WINDOW_H */
