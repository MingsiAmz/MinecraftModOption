#ifndef ROLLBACK_H
#define ROLLBACK_H

#include "mod_list.h"

typedef struct {
    char file_name[512];
    char version[64];
    char timestamp[64];
} BackupEntry;

typedef struct {
    BackupEntry entries[256];
    int count;
} BackupList;

/**
 * 列出指定模组的所有备份
 * @param backup_dir 备份目录
 * @param mod_name   模组名称
 * @param list       输出备份列表
 * @return 备份数量
 */
int rollback_list_backups(const char *backup_dir, const char *mod_name, BackupList *list);

/**
 * 回滚到指定备份
 * @param mod           模组信息
 * @param backup_path   备份文件完整路径
 * @param backup_dir    备份目录（用于自动保存当前版本）
 * @return 0成功，-1失败
 */
int rollback_to_backup(ModInfo *mod, const char *backup_path, const char *backup_dir);

#endif /* ROLLBACK_H */
