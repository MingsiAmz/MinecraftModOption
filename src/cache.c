#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

// ─── 内部辅助：从 mods_cache.json 读取 JSON 根对象 ───
static cJSON *cache_load_root(void)
{
    FILE *f = fopen(CACHE_FILE, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *json_str = malloc((size_t)len + 1);
    if (!json_str) { fclose(f); return NULL; }
    fread(json_str, 1, (size_t)len, f);
    json_str[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(json_str);
    free(json_str);
    return root;
}

static int cache_save_root(cJSON *root)
{
    char *json_str = cJSON_Print(root);
    if (!json_str) return -1;

    FILE *f = fopen(CACHE_FILE, "w");
    if (!f) { free(json_str); return -1; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return 0;
}

// ─── 保存模组列表到缓存 ───
int cache_save(GPtrArray *mods)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    // mods 数组
    cJSON *mods_arr = cJSON_AddArrayToObject(root, "mods");
    for (int i = 0; i < (int)mods->len; i++) {
        ModInfo *mod = (ModInfo *)g_ptr_array_index(mods, i);

        cJSON *m = cJSON_CreateObject();
        cJSON_AddStringToObject(m, "mod_id", mod->mod_id);
        cJSON_AddStringToObject(m, "name", mod->name);
        cJSON_AddStringToObject(m, "local_version", mod->local_version);
        cJSON_AddStringToObject(m, "latest_version", mod->latest_version);
        cJSON_AddStringToObject(m, "mc_version", mod->mc_version);
        cJSON_AddStringToObject(m, "loader", mod->loader);
        cJSON_AddNumberToObject(m, "source", mod->source);
        cJSON_AddNumberToObject(m, "status", mod->status);
        cJSON_AddStringToObject(m, "file_name", mod->file_name);
        cJSON_AddStringToObject(m, "file_path", mod->file_path);
        cJSON_AddNumberToObject(m, "file_size", mod->file_size);
        cJSON_AddStringToObject(m, "project_url", mod->project_url);

        cJSON_AddItemToArray(mods_arr, m);
    }

    int ret = cache_save_root(root);
    cJSON_Delete(root);
    return ret;
}

// ─── 从缓存加载模组列表 ───
int cache_load(GPtrArray *mods)
{
    cJSON *root = cache_load_root();
    if (!root) return -1;

    cJSON *mods_arr = cJSON_GetObjectItem(root, "mods");
    if (!mods_arr || !cJSON_IsArray(mods_arr)) {
        cJSON_Delete(root);
        return -1;
    }

    int count = 0;
    cJSON *m;
    cJSON_ArrayForEach(m, mods_arr) {
        ModInfo info;
        memset(&info, 0, sizeof(info));

        cJSON *val;
        if ((val = cJSON_GetObjectItem(m, "mod_id")) && cJSON_IsString(val))
            snprintf(info.mod_id, sizeof(info.mod_id), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "name")) && cJSON_IsString(val))
            snprintf(info.name, sizeof(info.name), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "local_version")) && cJSON_IsString(val))
            snprintf(info.local_version, sizeof(info.local_version), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "latest_version")) && cJSON_IsString(val))
            snprintf(info.latest_version, sizeof(info.latest_version), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "mc_version")) && cJSON_IsString(val))
            snprintf(info.mc_version, sizeof(info.mc_version), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "loader")) && cJSON_IsString(val))
            snprintf(info.loader, sizeof(info.loader), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "source")) && cJSON_IsNumber(val))
            info.source = val->valueint;
        if ((val = cJSON_GetObjectItem(m, "status")) && cJSON_IsNumber(val))
            info.status = val->valueint;
        if ((val = cJSON_GetObjectItem(m, "file_name")) && cJSON_IsString(val))
            snprintf(info.file_name, sizeof(info.file_name), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "file_path")) && cJSON_IsString(val))
            snprintf(info.file_path, sizeof(info.file_path), "%s", val->valuestring);
        if ((val = cJSON_GetObjectItem(m, "file_size")) && cJSON_IsNumber(val))
            info.file_size = (long)val->valuedouble;
        if ((val = cJSON_GetObjectItem(m, "project_url")) && cJSON_IsString(val))
            snprintf(info.project_url, sizeof(info.project_url), "%s", val->valuestring);

        ModInfo *copy = g_new(ModInfo, 1);
        memcpy(copy, &info, sizeof(ModInfo));
        g_ptr_array_add(mods, copy);
        count++;
    }

    cJSON_Delete(root);
    return count;
}

// ─── 获取远程版本信息 ───
int cache_get_remote_info(const char *mod_id, char *latest_version, char *project_url)
{
    cJSON *root = cache_load_root();
    if (!root) return -1;

    cJSON *remote_arr = cJSON_GetObjectItem(root, "remote_info");
    if (!remote_arr || !cJSON_IsArray(remote_arr)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item;
    cJSON_ArrayForEach(item, remote_arr) {
        cJSON *id_val = cJSON_GetObjectItem(item, "mod_id");
        if (id_val && cJSON_IsString(id_val) && strcmp(id_val->valuestring, mod_id) == 0) {
            cJSON *v = cJSON_GetObjectItem(item, "latest_version");
            if (v && cJSON_IsString(v))
                snprintf(latest_version, 64, "%s", v->valuestring);
            cJSON *u = cJSON_GetObjectItem(item, "project_url");
            if (u && cJSON_IsString(u))
                snprintf(project_url, 512, "%s", u->valuestring);
            cJSON_Delete(root);
            return 0;
        }
    }

    cJSON_Delete(root);
    return -1;
}

// ─── 更新远程版本信息 ───
void cache_update_remote(const char *mod_id, const char *latest_version, const char *project_url)
{
    cJSON *root = cache_load_root();
    int new_file = 0;
    if (!root) {
        root = cJSON_CreateObject();
        new_file = 1;
    }

    cJSON *remote_arr = cJSON_GetObjectItem(root, "remote_info");
    if (!remote_arr) {
        remote_arr = cJSON_AddArrayToObject(root, "remote_info");
    }

    // 检查是否已有该模组的记录
    cJSON *item;
    cJSON_ArrayForEach(item, remote_arr) {
        cJSON *id_val = cJSON_GetObjectItem(item, "mod_id");
        if (id_val && cJSON_IsString(id_val) && strcmp(id_val->valuestring, mod_id) == 0) {
            // 更新已有记录
            cJSON *v = cJSON_GetObjectItem(item, "latest_version");
            if (v) cJSON_SetValuestring(v, latest_version);
            else cJSON_AddStringToObject(item, "latest_version", latest_version);
            cJSON *u = cJSON_GetObjectItem(item, "project_url");
            if (u) cJSON_SetValuestring(u, project_url);
            else cJSON_AddStringToObject(item, "project_url", project_url);
            goto save;
        }
    }

    // 新增记录
    cJSON *new_item = cJSON_CreateObject();
    cJSON_AddStringToObject(new_item, "mod_id", mod_id);
    cJSON_AddStringToObject(new_item, "latest_version", latest_version);
    cJSON_AddStringToObject(new_item, "project_url", project_url);
    cJSON_AddItemToArray(remote_arr, new_item);

save:
    if (new_file) {
        // 确保有 mods 数组
        if (!cJSON_GetObjectItem(root, "mods"))
            cJSON_AddArrayToObject(root, "mods");
    }
    cache_save_root(root);
    cJSON_Delete(root);
}
