#include "rollback.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>

// ─── 列出备份 ───
int rollback_list_backups(const char *backup_dir, const char *mod_name, BackupList *list)
{
    memset(list, 0, sizeof(BackupList));

    char pattern[2048];
    snprintf(pattern, sizeof(pattern), "%s\\%s_*.jar", backup_dir, mod_name);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return 0;

    do {
        if (list->count >= 256) break;
        BackupEntry *entry = &list->entries[list->count];
        snprintf(entry->file_name, sizeof(entry->file_name), "%s", find_data.cFileName);

        // 从文件名解析版本和时间戳
        // 格式: 模组名_版本_YYYYMMDD_HHMMSS.jar
        char *p = entry->file_name;
        // 跳过模组名
        p = strchr(p, '_');
        if (p) {
            p++; // 跳过第一个 _
            // 复制版本
            int vi = 0;
            while (*p && *p != '_' && vi < 63)
                entry->version[vi++] = *p++;
            entry->version[vi] = '\0';
            if (*p == '_') p++;

            // 复制时间戳
            int ti = 0;
            while (*p && *p != '.' && ti < 63)
                entry->timestamp[ti++] = *p++;
            entry->timestamp[ti] = '\0';

            // 格式化为可读时间
            if (strlen(entry->timestamp) >= 15) {
                char formatted[32];
                snprintf(formatted, sizeof(formatted),
                         "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
                         entry->timestamp,
                         entry->timestamp + 4,
                         entry->timestamp + 6,
                         entry->timestamp + 9,
                         entry->timestamp + 11,
                         entry->timestamp + 13);
                snprintf(entry->timestamp, sizeof(entry->timestamp), "%s", formatted);
            }
        }

        list->count++;
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    return list->count;
}

// ─── 回滚到指定备份 ───
int rollback_to_backup(ModInfo *mod, const char *backup_path, const char *backup_dir)
{
    // 1. 先备份当前版本
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "if not exist \"%s\" mkdir \"%s\"", backup_dir, backup_dir);
    system(cmd);

    char current_backup[2048];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(current_backup, sizeof(current_backup),
             "%s\\%s_%s_%04d%02d%02d_%02d%02d%02d_pre_rollback.jar",
             backup_dir, mod->name, mod->local_version,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    CopyFileA(mod->file_path, current_backup, FALSE);

    // 2. 备份原文件（防意外）
    char old_file[1024];
    snprintf(old_file, sizeof(old_file), "%s.old", mod->file_path);
    remove(old_file);

    if (!MoveFileExA(mod->file_path, old_file,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return -1;

    // 3. 复制备份版本到模组目录
    if (!CopyFileA(backup_path, mod->file_path, FALSE)) {
        // 恢复
        MoveFileExA(old_file, mod->file_path,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        return -1;
    }

    // 4. 删除 .old 文件
    remove(old_file);

    // 5. 更新模组信息：从备份文件名提取版本
    // 格式: 模组名_版本_时间戳.jar
    const char *fname = strrchr(backup_path, '\\');
    if (!fname) fname = strrchr(backup_path, '/');
    if (!fname) fname = backup_path;
    else fname++;

    const char *first_underscore = strchr(fname, '_');
    if (first_underscore) {
        const char *version_start = first_underscore + 1;
        const char *second_underscore = strchr(version_start, '_');
        if (second_underscore) {
            size_t ver_len = second_underscore - version_start;
            if (ver_len < sizeof(mod->local_version)) {
                strncpy(mod->local_version, version_start, ver_len);
                mod->local_version[ver_len] = '\0';
            }
        }
    }

    mod->status = MOD_STATUS_UNKNOWN;
    mod->latest_version[0] = '\0';

    return 0;
}
