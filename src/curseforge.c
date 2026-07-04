#include "curseforge.h"
#include "network.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static const char **build_headers(const char *api_key)
{
    static const char *headers[4];
    int i = 0;

    static char auth_header[512];
    if (api_key && api_key[0]) {
        snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);
        headers[i++] = auth_header;
    }

    headers[i++] = "User-Agent: MingsiAmz/MinecraftModManager/1.0";
    headers[i++] = "Accept: application/json";
    headers[i] = NULL;

    return headers;
}

// ─── 通过模组 ID 查询最新版本 ───
int curseforge_query_version(ModInfo *mod, const char *api_key, const char *mc_version)
{
    if (!api_key || !api_key[0]) {
        // 无 API Key 无法查询
        return -1;
    }

    char url[2048];
    snprintf(url, sizeof(url),
        "%s/mods/%s/files?gameVersion=%s&sortOrder=desc&pageSize=1",
        CURSEFORGE_API_BASE, mod->mod_id,
        mc_version ? mc_version : "");

    MemoryBuffer resp = {0};
    if (network_get(url, build_headers(api_key), &resp) != 0) {
        free(resp.data);
        return -1;
    }

    cJSON *root = cJSON_Parse(resp.data);
    free(resp.data);

    if (!root) return -1;

    // CurseForge 响应结构: { "data": [ ... ] }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *file = cJSON_GetArrayItem(data, 0);
    if (!file) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *val;

    // 显示名称
    if ((val = cJSON_GetObjectItem(file, "displayName")) && cJSON_IsString(val)) {
        // 版本号通常在 displayName 中，如 "jei-16.0.0.5"
        // 同时也尝试 fileName
    }

    // 文件名（包含版本）
    if ((val = cJSON_GetObjectItem(file, "fileName")) && cJSON_IsString(val)) {
        // 从文件名提取版本号
        // 例如 "jei-16.0.0.5.jar"
        const char *fn = val->valuestring;
        // 尝试提取版本
        char version_buf[64] = "";
        const char *dash = strchr(fn, '-');
        if (dash) {
            dash++; // 跳过 '-'
            // 复制到 .jar 或结尾
            int vi = 0;
            while (*dash && *dash != '.' && vi < 63) {
                // 版本号通常包含数字和点
                if ((*dash >= '0' && *dash <= '9') || *dash == '.')
                    version_buf[vi++] = *dash;
                else
                    break;
                dash++;
            }
            version_buf[vi] = '\0';
        }
        if (version_buf[0])
            snprintf(mod->latest_version, sizeof(mod->latest_version), "%s", version_buf);
    }

    // 文件 ID（可用于下载）
    if ((val = cJSON_GetObjectItem(file, "id")) && cJSON_IsNumber(val)) {
        // 构造项目 URL
        snprintf(mod->project_url, sizeof(mod->project_url),
                 "https://www.curseforge.com/minecraft/mc-mods/%s", mod->mod_id);
    }

    cJSON_Delete(root);

    // 比对版本
    if (mod->latest_version[0]) {
        if (strcmp(mod->local_version, mod->latest_version) == 0) {
            mod->status = MOD_STATUS_UP_TO_DATE;
        } else {
            mod->status = MOD_STATUS_UPDATE_AVAIL;
        }
        mod->source = MOD_SOURCE_CURSEFORGE;

        cache_update_remote(mod->mod_id, mod->latest_version, mod->project_url);
        return 0;
    }

    return -1;
}

// ─── 搜索模组 ───
int curseforge_search(const char *name, ModInfo *mod, const char *api_key)
{
    if (!api_key || !api_key[0]) return -1;

    char url[2048];
    // URL 编码省略，简化为直接替换空格
    char search_term[256];
    snprintf(search_term, sizeof(search_term), "%s", name);
    for (char *p = search_term; *p; p++)
        if (*p == ' ') *p = '+';

    snprintf(url, sizeof(url),
        "%s/mods/search?searchFilter=%s&sortOrder=desc&pageSize=1",
        CURSEFORGE_API_BASE, search_term);

    MemoryBuffer resp = {0};
    if (network_get(url, build_headers(api_key), &resp) != 0) {
        free(resp.data);
        return -1;
    }

    cJSON *root = cJSON_Parse(resp.data);
    free(resp.data);

    if (!root) return -1;

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item = cJSON_GetArrayItem(data, 0);
    if (!item) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *val;
    if ((val = cJSON_GetObjectItem(item, "id")) && cJSON_IsNumber(val)) {
        char id_str[32];
        snprintf(id_str, sizeof(id_str), "%d", val->valueint);
        snprintf(mod->mod_id, sizeof(mod->mod_id), "%s", id_str);
    }
    if ((val = cJSON_GetObjectItem(item, "name")) && cJSON_IsString(val))
        snprintf(mod->name, sizeof(mod->name), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(item, "slug")) && cJSON_IsString(val))
        snprintf(mod->project_url, sizeof(mod->project_url),
                 "https://www.curseforge.com/minecraft/mc-mods/%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(item, "summary")) && cJSON_IsString(val))
        snprintf(mod->description, sizeof(mod->description), "%s", val->valuestring);

    cJSON_Delete(root);

    if (mod->mod_id[0])
        return curseforge_query_version(mod, api_key, NULL);

    return -1;
}
