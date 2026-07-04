#include "deleter.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

int deleter_delete_mod(ModInfo *mod, const char *backup_dir, bool keep_backup)
{
    // 如果需要保留备份，先复制到备份目录
    if (keep_backup && backup_dir && backup_dir[0]) {
        // 确保备份目录存在
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "if not exist \"%s\" mkdir \"%s\"", backup_dir, backup_dir);
        system(cmd);

        char backup_path[2048];
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        snprintf(backup_path, sizeof(backup_path),
                 "%s\\%s_%s_%04d%02d%02d_%02d%02d%02d.deleted.jar",
                 backup_dir, mod->name, mod->local_version,
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);

        CopyFileA(mod->file_path, backup_path, FALSE);
    }

    // 删除文件
    if (!DeleteFileA(mod->file_path)) {
        return -1;
    }

    return 0;
}

int deleter_delete_batch(int *indices, int count,
                          const char *backup_dir, bool keep_backup,
                          int *success_count)
{
    int deleted = 0;
    for (int i = 0; i < count; i++) {
        ModInfo *mod = mod_list_get(indices[i]);
        if (!mod) continue;

        if (deleter_delete_mod(mod, backup_dir, keep_backup) == 0) {
            deleted++;
        }
    }

    if (success_count) *success_count = deleted;
    return deleted;
}
