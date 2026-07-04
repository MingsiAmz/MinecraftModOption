#ifndef DELETER_H
#define DELETER_H

#include "mod_list.h"
#include <stdbool.h>

/**
 * 删除单个模组
 * @param mod           模组信息
 * @param backup_dir    备份目录（如果保留备份）
 * @param keep_backup   是否保留备份
 * @return 0成功，-1失败
 */
int deleter_delete_mod(ModInfo *mod, const char *backup_dir, bool keep_backup);

/**
 * 批量删除模组
 * @param indices       要删除的模组索引数组
 * @param count         数量
 * @param backup_dir    备份目录
 * @param keep_backup   是否保留备份
 * @param success_count 输出成功删除数
 * @return 删除数量
 */
int deleter_delete_batch(int *indices, int count,
                          const char *backup_dir, bool keep_backup,
                          int *success_count);

#endif /* DELETER_H */
