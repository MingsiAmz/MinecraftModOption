#include "updater.h"
#include "network.h"
#include "hash.h"
#include "cache.h"
#include "modrinth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <windows.h>
#include <cjson/cJSON.h>

// ─── 备份一个模组文件 ───
static int backup_mod_file(const char *file_path, const char *backup_dir,
                            const char *mod_name, const char *version)
{
    // 确保备份目录存在
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "if not exist \"%s\" mkdir \"%s\"", backup_dir, backup_dir);
    system(cmd);

    // 构造备份文件名：模组名_版本_时间戳.jar
    char backup_name[1024];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(backup_name, sizeof(backup_name), "%s_%s_%04d%02d%02d_%02d%02d%02d.jar",
             mod_name, version,
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);

    char backup_path[2048];
    snprintf(backup_path, sizeof(backup_path), "%s\\%s", backup_dir, backup_name);

    // 复制文件
    if (!CopyFileA(file_path, backup_path, FALSE)) {
        return -1;
    }
    return 0;
}

// ─── 清理旧备份（超出 max_backups） ───
static void cleanup_old_backups(const char *backup_dir, const char *mod_name, int max_backups)
{
    if (max_backups <= 0) return;

    // 列出备份目录下属于该模组的所有备份文件
    char pattern[2048];
    snprintf(pattern, sizeof(pattern), "%s\\%s_*.jar", backup_dir, mod_name);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(pattern, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return;

    // 收集所有备份文件
    typedef struct {
        char name[512];
        FILETIME time;
    } BackupEntry;

    BackupEntry entries[256];
    int count = 0;

    do {
        if (count < 256) {
            snprintf(entries[count].name, sizeof(entries[count].name), "%s", find_data.cFileName);
            entries[count].time = find_data.ftCreationTime;
            count++;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);

    if (count <= max_backups) return;

    // 按时间排序（冒泡，数量少无所谓）
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (CompareFileTime(&entries[j].time, &entries[j+1].time) > 0) {
                BackupEntry tmp = entries[j];
                entries[j] = entries[j+1];
                entries[j+1] = tmp;
            }
        }
    }

    // 删除最旧的（超出 max_backups 的部分）
    int to_delete = count - max_backups;
    for (int i = 0; i < to_delete; i++) {
        char del_path[2048];
        snprintf(del_path, sizeof(del_path), "%s\\%s", backup_dir, entries[i].name);
        DeleteFileA(del_path);
    }
}

// ─── 更新单个模组 ───
int updater_update_mod(ModInfo *mod, const char *backup_dir,
                        bool backup_before, int max_backups)
{
    // 如果没有最新版本信息，无法更新
    if (mod->latest_version[0] == '\0')
        return -1;

    // 更新前备份
    if (backup_before) {
        if (backup_mod_file(mod->file_path, backup_dir, mod->name, mod->local_version) == 0) {
            mod->backup_count++;
            cleanup_old_backups(backup_dir, mod->name, max_backups);
        }
    }

    // 构造下载 URL（根据来源平台）
    char download_url[2048] = "";

    if (mod->source == MOD_SOURCE_MODRINTH) {
        // Modrinth 下载：需先查询具体版本文件
        char api_url[2048];
        snprintf(api_url, sizeof(api_url),
                 "%s/project/%s/version?loaders=[\"%s\"]",
                 MODRINTH_API_BASE, mod->mod_id,
                 mod->loader[0] ? mod->loader : "fabric");

        MemoryBuffer resp = {0};
        const char *headers[] = {
            "User-Agent: MingsiAmz/MinecraftModManager/1.0",
            "Accept: application/json",
            NULL
        };

        if (network_get(api_url, headers, &resp) == 0) {
            cJSON *versions = cJSON_Parse(resp.data);
            if (versions && cJSON_IsArray(versions)) {
                cJSON *v = cJSON_GetArrayItem(versions, 0);
                if (v) {
                    cJSON *files = cJSON_GetObjectItem(v, "files");
                    if (files && cJSON_IsArray(files)) {
                        cJSON *file = cJSON_GetArrayItem(files, 0);
                        if (file) {
                            cJSON *url_val = cJSON_GetObjectItem(file, "url");
                            if (url_val && cJSON_IsString(url_val))
                                snprintf(download_url, sizeof(download_url), "%s", url_val->valuestring);
                        }
                    }
                }
            }
            cJSON_Delete(versions);
        }
        free(resp.data);
    } else if (mod->source == MOD_SOURCE_CURSEFORGE) {
        // CurseForge 下载需要 API Key + 文件 ID
        // 暂未实现（需要更多 API 调用链）
        // 可以从项目主页获取下载链接
        snprintf(download_url, sizeof(download_url), "");
    }

    if (download_url[0] == '\0') {
        // 无法获取下载 URL，失败
        return -1;
    }

    // 下载到临时文件
    char tmp_file[1024];
    snprintf(tmp_file, sizeof(tmp_file), "%s.download.tmp", mod->file_path);

    if (network_download(download_url, tmp_file) != 0) {
        remove(tmp_file);
        return -1;
    }

    // SHA1 校验（可选，如果有校验值）
    // 替换原文件
    char old_file[1024];
    snprintf(old_file, sizeof(old_file), "%s.old", mod->file_path);
    remove(old_file); // 清理上次残留

    if (!MoveFileExA(mod->file_path, old_file,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        remove(tmp_file);
        return -1;
    }

    if (!MoveFileExA(tmp_file, mod->file_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // 恢复
        MoveFileExA(old_file, mod->file_path,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        remove(tmp_file);
        return -1;
    }

    // 删除旧文件
    remove(old_file);

    // 更新模组信息
    snprintf(mod->local_version, sizeof(mod->local_version), "%s", mod->latest_version);
    mod->latest_version[0] = '\0'; // 清空，后续重新查询
    mod->status = MOD_STATUS_UP_TO_DATE;

    // 更新文件大小
    struct stat st;
    if (stat(mod->file_path, &st) == 0)
        mod->file_size = (long)st.st_size;

    return 0;
}

// ─── 批量更新 ───
void updater_update_batch(int *indices, int count,
                           const char *backup_dir, bool backup_before,
                           int max_backups, UpdateResult *result)
{
    memset(result, 0, sizeof(UpdateResult));
    result->total = count;

    for (int i = 0; i < count; i++) {
        ModInfo *mod = mod_list_get(indices[i]);
        if (!mod) {
            result->failed++;
            snprintf(result->errors[result->error_count++], 512,
                     "索引 %d 无效", indices[i]);
            continue;
        }

        if (updater_update_mod(mod, backup_dir, backup_before, max_backups) == 0) {
            result->success++;
        } else {
            result->failed++;
            if (result->error_count < 10) {
                snprintf(result->errors[result->error_count++], 512,
                         "%s: 更新失败", mod->name);
            }
        }
    }
}
