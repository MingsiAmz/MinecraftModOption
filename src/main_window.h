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

#endif /* MAIN_WINDOW_H */
