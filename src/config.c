#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cjson/cJSON.h>

void config_set_defaults(AppConfig *config)
{
    // 默认模组目录：用户 home 下的 .minecraft/mods
    const char *home = getenv("USERPROFILE");
    if (home) {
        snprintf(config->mod_dir, sizeof(config->mod_dir),
                 "%s\\AppData\\Roaming\\.minecraft\\mods", home);
    } else {
        snprintf(config->mod_dir, sizeof(config->mod_dir), "mods");
    }

    // 默认备份目录：模组目录下的 backups
    snprintf(config->backup_dir, sizeof(config->backup_dir),
             "%s\\backups", config->mod_dir);

    config->max_backups = 5;
    config->curseforge_api_key[0] = '\0';
    config->auto_scan_on_start = true;
    config->backup_before_update = true;
}

int config_save(const AppConfig *config)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON_AddStringToObject(root, "mod_dir", config->mod_dir);
    cJSON_AddStringToObject(root, "backup_dir", config->backup_dir);
    cJSON_AddNumberToObject(root, "max_backups", config->max_backups);
    cJSON_AddStringToObject(root, "curseforge_api_key", config->curseforge_api_key);
    cJSON_AddBoolToObject(root, "auto_scan_on_start", config->auto_scan_on_start);
    cJSON_AddBoolToObject(root, "backup_before_update", config->backup_before_update);

    char *json_str = cJSON_Print(root);
    if (!json_str) {
        cJSON_Delete(root);
        return -1;
    }

    FILE *f = fopen(CONFIG_FILE, "w");
    if (!f) {
        free(json_str);
        cJSON_Delete(root);
        return -1;
    }
    fputs(json_str, f);
    fclose(f);

    free(json_str);
    cJSON_Delete(root);
    return 0;
}

static void ensure_backup_dir(const AppConfig *config)
{
    // 确保备份目录存在（简单实现，Windows下调用 mkdir）
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "if not exist \"%s\" mkdir \"%s\"",
             config->backup_dir, config->backup_dir);
    system(cmd);
}

int config_load(AppConfig *config)
{
    config_set_defaults(config);

    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) {
        // 配置文件不存在，创建默认配置
        ensure_backup_dir(config);
        config_save(config);
        return 0;
    }

    // 读取文件内容
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *json_str = malloc((size_t)len + 1);
    if (!json_str) {
        fclose(f);
        return -1;
    }
    fread(json_str, 1, (size_t)len, f);
    json_str[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);

    if (!root) {
        // JSON 解析失败，使用默认
        ensure_backup_dir(config);
        config_save(config);
        return 0;
    }

    cJSON *val;
    if ((val = cJSON_GetObjectItem(root, "mod_dir")) && cJSON_IsString(val))
        snprintf(config->mod_dir, sizeof(config->mod_dir), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(root, "backup_dir")) && cJSON_IsString(val))
        snprintf(config->backup_dir, sizeof(config->backup_dir), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(root, "max_backups")) && cJSON_IsNumber(val))
        config->max_backups = val->valueint;
    if ((val = cJSON_GetObjectItem(root, "curseforge_api_key")) && cJSON_IsString(val))
        snprintf(config->curseforge_api_key, sizeof(config->curseforge_api_key), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(root, "auto_scan_on_start")) && cJSON_IsBool(val))
        config->auto_scan_on_start = cJSON_IsTrue(val);
    if ((val = cJSON_GetObjectItem(root, "backup_before_update")) && cJSON_IsBool(val))
        config->backup_before_update = cJSON_IsTrue(val);

    cJSON_Delete(root);
    ensure_backup_dir(config);
    return 0;
}
