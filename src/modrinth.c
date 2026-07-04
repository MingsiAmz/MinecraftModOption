#include "modrinth.h"
#include "network.h"
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <cjson/cJSON.h>

// ─── 构建 Modrinth API 请求头 ───
static const char **build_headers(void)
{
    static const char *headers[] = {
        "User-Agent: MingsiAmz/MinecraftModManager/1.0 (Contact: user@example.com)",
        "Accept: application/json",
        NULL
    };
    return headers;
}

// ─── 通过项目 ID 查询版本 ───
int modrinth_query_version(ModInfo *mod, const char *mc_version, const char *loader)
{
    char url[2048];

    // Modrinth API v2 查询版本列表
    // URL: /v2/project/{slug}/version
    // 返回所有版本的 JSON 数组，取第一个（最新）
    snprintf(url, sizeof(url), "%s/project/%s/version",
        MODRINTH_API_BASE, mod->mod_id);

    MemoryBuffer resp = {0};
    if (network_get(url, build_headers(), &resp) != 0) {
        free(resp.data);
        return -1;
    }

    // 解析响应（JSON 数组）
    cJSON *versions = cJSON_Parse(resp.data);
    free(resp.data);

    if (!versions || !cJSON_IsArray(versions)) {
        if (versions) cJSON_Delete(versions);
        return -1;
    }

    // 遍历版本列表，尝试匹配 MC 版本和加载器
    cJSON *best_match = NULL;
    cJSON *first = NULL;
    int version_count = cJSON_GetArraySize(versions);

    for (int i = 0; i < version_count; i++) {
        cJSON *v = cJSON_GetArrayItem(versions, i);
        if (!v) continue;
        if (!first) first = v;

        // 如果有 filter 参数，尝试匹配
        if (mc_version || loader) {
            int mc_ok = 1, loader_ok = 1;

            if (mc_version) {
                cJSON *gv = cJSON_GetObjectItem(v, "game_versions");
                mc_ok = 0;
                if (gv && cJSON_IsArray(gv)) {
                    int gv_count = cJSON_GetArraySize(gv);
                    for (int j = 0; j < gv_count; j++) {
                        cJSON *gv_item = cJSON_GetArrayItem(gv, j);
                        if (gv_item && cJSON_IsString(gv_item)
                            && strstr(gv_item->valuestring, mc_version)) {
                            mc_ok = 1;
                            break;
                        }
                    }
                }
            }

            if (loader && mc_ok) {
                cJSON *ld = cJSON_GetObjectItem(v, "loaders");
                loader_ok = 0;
                if (ld && cJSON_IsArray(ld)) {
                    int ld_count = cJSON_GetArraySize(ld);
                    for (int j = 0; j < ld_count; j++) {
                        cJSON *ld_item = cJSON_GetArrayItem(ld, j);
                        if (ld_item && cJSON_IsString(ld_item)
                            && strcasecmp(ld_item->valuestring, loader) == 0) {
                            loader_ok = 1;
                            break;
                        }
                    }
                }
            }

            if (mc_ok && loader_ok) {
                best_match = v;
                break; // 找到最匹配的
            }
        }
    }

    cJSON *target = best_match ? best_match : first;
    if (!target) {
        cJSON_Delete(versions);
        return -1;
    }

    cJSON *val;
    if ((val = cJSON_GetObjectItem(target, "version_number")) && cJSON_IsString(val))
        snprintf(mod->latest_version, sizeof(mod->latest_version), "%s", val->valuestring);

    if ((val = cJSON_GetObjectItem(target, "id")) && cJSON_IsString(val)) {
        if (!mod->project_url[0])
            snprintf(mod->project_url, sizeof(mod->project_url),
                     "https://modrinth.com/mod/%s", mod->mod_id);
    }

    // 比对版本号
    if (mod->latest_version[0]) {
        if (strcmp(mod->local_version, mod->latest_version) == 0) {
            mod->status = MOD_STATUS_UP_TO_DATE;
        } else {
            mod->status = MOD_STATUS_UPDATE_AVAIL;
        }
        mod->source = MOD_SOURCE_MODRINTH;
    }

    cJSON_Delete(versions);

    // 缓存远程信息
    if (mod->latest_version[0])
        cache_update_remote(mod->mod_id, mod->latest_version, mod->project_url);

    return 0;
}

// ─── 通过 slug 搜索 ───
int modrinth_search_slug(const char *slug, ModInfo *mod)
{
    char url[2048];
    snprintf(url, sizeof(url),
        "%s/project/%s",
        MODRINTH_API_BASE, slug);

    MemoryBuffer resp = {0};
    if (network_get(url, build_headers(), &resp) != 0) {
        free(resp.data);
        return -1;
    }

    cJSON *project = cJSON_Parse(resp.data);
    free(resp.data);

    if (!project) return -1;

    cJSON *val;
    if ((val = cJSON_GetObjectItem(project, "id")) && cJSON_IsString(val))
        snprintf(mod->mod_id, sizeof(mod->mod_id), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(project, "title")) && cJSON_IsString(val))
        snprintf(mod->name, sizeof(mod->name), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(project, "description")) && cJSON_IsString(val))
        snprintf(mod->description, sizeof(mod->description), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(project, "slug")) && cJSON_IsString(val))
        snprintf(mod->project_url, sizeof(mod->project_url),
                 "https://modrinth.com/mod/%s", val->valuestring);

    // 获取最新版本
    cJSON_Delete(project);

    if (mod->mod_id[0])
        return modrinth_query_version(mod, NULL, NULL);

    return -1;
}
