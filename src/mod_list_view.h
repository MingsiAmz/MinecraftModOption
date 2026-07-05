#ifndef MOD_LIST_VIEW_H
#define MOD_LIST_VIEW_H

#include <gtk/gtk.h>
#include "mod_list.h"

/**
 * 创建模组列表视图（GtkColumnView）
 * @return GtkWidget 列表视图
 */
GtkWidget *mod_list_view_create(void);

/**
 * 刷新列表显示（从 mod_list 数据重新填充）
 */
void mod_list_view_refresh(void);

/**
 * 获取选中模组的索引数组
 * @param count 输出选中数量
 * @return 索引数组，需 free
 */
int *mod_list_view_get_selected(int *count);

/**
 * 全选/取消全选
 * @param select TRUE 全选，FALSE 取消全选
 */
void mod_list_view_select_all(gboolean select);

/**
 * 是否有任意模组被勾选
 * @return TRUE 至少有一个勾选
 */
gboolean mod_list_view_has_any_checked(void);

/**
 * 设置搜索文本和筛选条件
 * @param search_text 搜索文本（可为NULL）
 * @param filter 筛选条件（可为NULL）
 */
void mod_list_view_set_filter(const char *search_text, const char *filter);

#endif /* MOD_LIST_VIEW_H */
