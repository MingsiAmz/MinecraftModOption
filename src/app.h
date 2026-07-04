#ifndef APP_H
#define APP_H

#include <gtk/gtk.h>
#include "config.h"

/**
 * 应用全局状态
 */
typedef struct {
    AppConfig config;
    gboolean is_scanning;
    gboolean is_updating;
} AppState;

/**
 * 获取全局应用状态
 */
AppState *app_get_state(void);

/**
 * 初始化应用
 * @return 0成功，-1失败
 */
int app_init(void);

/**
 * 清理应用资源
 */
void app_cleanup(void);

#endif /* APP_H */
