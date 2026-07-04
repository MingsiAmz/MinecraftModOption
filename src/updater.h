#ifndef UPDATER_H
#define UPDATER_H

#include "mod_list.h"

typedef struct {
    int total;
    int success;
    int failed;
    char errors[10][512];  // 最多记录10个错误
    int error_count;
} UpdateResult;

/**
 * 更新单个模组
 * @param mod           模组信息
 * @param backup_dir    备份目录
 * @param backup_before 更新前是否备份
 * @param max_backups   最大备份数
 * @return 0成功，-1失败
 */
int updater_update_mod(ModInfo *mod, const char *backup_dir,
                        bool backup_before, int max_backups);

/**
 * 批量更新多个模组
 * @param indices       要更新的模组索引数组
 * @param count         数量
 * @param backup_dir    备份目录
 * @param backup_before 更新前是否备份
 * @param max_backups   最大备份数
 * @param result        输出结果
 */
void updater_update_batch(int *indices, int count,
                           const char *backup_dir, bool backup_before,
                           int max_backups, UpdateResult *result);

#endif /* UPDATER_H */
