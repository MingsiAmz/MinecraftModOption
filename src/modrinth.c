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

/* ══════════════════════════════════════════════════════════════════
 * 批量接口：用文件 SHA1 识别模组并检测更新
 *
 *   POST /version_files         hashes + algorithm         → hash → version
 *   POST /version_files/update  hashes + loaders + 游戏版本 → hash → 最新版本
 *   GET  /projects?ids=[...]    批量取标题
 * ══════════════════════════════════════════════════════════════════ */

#define BULK_CHUNK 100   /* 单次 /version_files 请求的最大 hash 数量 */
#define MAX_LOADERS 8

typedef struct {
    ModInfo *mod;
    char hash[48];
    char project_id[40];
    char local_id[40];
    char local_version[64];
    char loader[32];
    char mc_version[32];
    char latest_id[48];
    char latest_version[64];
    char latest_sha1[48];
    bool identified;
    bool has_update;
    bool update_checked;
} ResolveEntry;

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (!src) src = "";
    snprintf(dst, dst_size, "%s", src);
}

/* ─── POST JSON 到 Modrinth ─── */
static cJSON *post_json(const char *path, cJSON *body)
{
    char url[512];
    snprintf(url, sizeof(url), "%s%s", MODRINTH_API_BASE, path);

    char *payload = cJSON_PrintUnformatted(body);
    if (!payload) return NULL;

    const char *headers[] = {
        "User-Agent: MingsiAmz/MinecraftModManager/1.0 (Contact: user@example.com)",
        "Accept: application/json",
        "Content-Type: application/json",
        NULL
    };

    MemoryBuffer resp = {0};
    long http_code = 0;
    int rc = network_post_json(url, headers, payload, &resp, &http_code);
    free(payload);

    cJSON *root = NULL;
    if (rc == 0 && http_code >= 200 && http_code < 300 && resp.data)
        root = cJSON_Parse(resp.data);

    free(resp.data);
    return root;
}

/* ─── 从 version 对象中取字符串字段 ─── */
static const char *json_str(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return (item && cJSON_IsString(item)) ? item->valuestring : NULL;
}

/* ─── 记录版本里的加载器与支持的游戏版本（取最高版本） ─── */
static void fill_version_meta(cJSON *version, char *loader, size_t loader_size,
                              char *mc_version, size_t mc_size)
{
    cJSON *item;

    if ((item = cJSON_GetObjectItemCaseSensitive(version, "loaders"))
        && cJSON_IsArray(item) && cJSON_GetArraySize(item) > 0) {
        cJSON *first = cJSON_GetArrayItem(item, 0);
        if (cJSON_IsString(first)) copy_str(loader, loader_size, first->valuestring);
    }

    if ((item = cJSON_GetObjectItemCaseSensitive(version, "game_versions"))
        && cJSON_IsArray(item)) {
        int n = cJSON_GetArraySize(item);
        if (n > 0) {
            cJSON *last = cJSON_GetArrayItem(item, n - 1);
            if (cJSON_IsString(last)) copy_str(mc_version, mc_size, last->valuestring);
        }
    }
}

/* ─── 取版本的主文件下载地址与 SHA1 ─── */
static void fill_file_info(cJSON *version, char *url_out, size_t url_size,
                           char *sha1_out, size_t sha1_size)
{
    cJSON *files = cJSON_GetObjectItemCaseSensitive(version, "files");
    if (!files || !cJSON_IsArray(files) || cJSON_GetArraySize(files) == 0) return;

    cJSON *picked = cJSON_GetArrayItem(files, 0);
    int count = cJSON_GetArraySize(files);
    for (int i = 0; i < count; i++) {
        cJSON *f = cJSON_GetArrayItem(files, i);
        cJSON *primary = cJSON_GetObjectItemCaseSensitive(f, "primary");
        if (primary && cJSON_IsBool(primary) && cJSON_IsTrue(primary)) {
            picked = f;
            break;
        }
    }

    const char *url = json_str(picked, "url");
    if (url) copy_str(url_out, url_size, url);

    cJSON *hashes = cJSON_GetObjectItemCaseSensitive(picked, "hashes");
    if (hashes) {
        const char *sha1 = json_str(hashes, "sha1");
        if (sha1) copy_str(sha1_out, sha1_size, sha1);
    }
}

/* ─── 解析识别阶段返回的 version 对象 ─── */
static void fill_identification(ResolveEntry *e, cJSON *version)
{
    const char *id = json_str(version, "id");
    const char *project_id = json_str(version, "project_id");
    const char *version_number = json_str(version, "version_number");

    if (!project_id) return;

    copy_str(e->project_id, sizeof(e->project_id), project_id);
    copy_str(e->local_id, sizeof(e->local_id), id ? id : "");
    copy_str(e->local_version, sizeof(e->local_version),
             version_number ? version_number : "");

    fill_version_meta(version, e->loader, sizeof(e->loader),
                      e->mc_version, sizeof(e->mc_version));

    /* 扫描器已经解析出的信息更可靠时予以保留 */
    if (!e->loader[0] && e->mod->loader[0])
        copy_str(e->loader, sizeof(e->loader), e->mod->loader);
    if (!e->mc_version[0] && e->mod->mc_version[0])
        copy_str(e->mc_version, sizeof(e->mc_version), e->mod->mc_version);

    e->identified = true;
}

/* ─── 版本号比较（仅用于挑选「更像实例版本」的一项） ─── */
static int version_compare(const char *a, const char *b)
{
    int i = 0, j = 0;
    while (a[i] && b[j]) {
        if (g_ascii_isdigit(a[i]) && g_ascii_isdigit(b[j])) {
            long va = strtol(a + i, NULL, 10);
            long vb = strtol(b + j, NULL, 10);
            if (va != vb) return (va < vb) ? -1 : 1;
            while (g_ascii_isdigit(a[i])) i++;
            while (g_ascii_isdigit(b[j])) j++;
        } else {
            if (a[i] != b[j]) return (a[i] < b[j]) ? -1 : 1;
            i++; j++;
        }
    }
    if (a[i]) return 1;
    if (b[j]) return -1;
    return 0;
}

/* ─── 统计出现次数最多的游戏版本，作为当前实例的 MC 版本 ─── */
static void detect_instance_version(ResolveEntry *entries, int count,
                                    char *out, size_t out_size)
{
    GHashTable *tally = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (int i = 0; i < count; i++) {
        if (!entries[i].identified || !entries[i].mc_version[0]) continue;
        char *key = g_strdup(entries[i].mc_version);
        int cur = GPOINTER_TO_INT(g_hash_table_lookup(tally, key));
        g_hash_table_replace(tally, key, GINT_TO_POINTER(cur + 1));
    }

    out[0] = '\0';
    int best_hits = 0;

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, tally);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        int hits = GPOINTER_TO_INT(value);
        const char *ver = (const char *)key;
        if (hits > best_hits
            || (hits == best_hits && out[0] && version_compare(ver, out) > 0)) {
            best_hits = hits;
            copy_str(out, out_size, ver);
        }
    }

    g_hash_table_destroy(tally);
}

/* ─── 收集所有加载器（去重） ─── */
static int collect_loaders(ResolveEntry *entries, int count, const char *loaders[])
{
    int loader_count = 0;

    for (int i = 0; i < count; i++) {
        if (!entries[i].identified || !entries[i].loader[0]) continue;

        bool seen = false;
        for (int j = 0; j < loader_count; j++) {
            if (strcmp(loaders[j], entries[i].loader) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen && loader_count < MAX_LOADERS)
            loaders[loader_count++] = entries[i].loader;
    }

    return loader_count;
}

/* ─── 批量拉取项目名称 ─── */
static void fetch_project_names(ResolveEntry *entries, int count)
{
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (int i = 0; i < count; i++) {
        if (!entries[i].identified || !entries[i].project_id[0]) continue;
        if (!g_hash_table_contains(seen, entries[i].project_id))
            g_hash_table_add(seen, g_strdup(entries[i].project_id));
    }

    if (g_hash_table_size(seen) == 0) {
        g_hash_table_destroy(seen);
        return;
    }

    GList *keys = g_hash_table_get_keys(seen);
    int total = (int)g_list_length(keys);

    for (int start = 0; start < total; start += 50) {
        int end = start + 50;
        if (end > total) end = total;

        GString *url = g_string_new(MODRINTH_API_BASE "/projects?ids=[");
        int pos = start;
        for (GList *l = g_list_nth(keys, start); l && pos < end; l = l->next, pos++) {
            if (pos > start) g_string_append_c(url, ',');
            g_string_append_printf(url, "%%22%s%%22", (const char *)l->data);
        }
        g_string_append_c(url, ']');

        MemoryBuffer resp = {0};
        if (network_get(url->str, build_headers(), &resp) == 0) {
            cJSON *projects = cJSON_Parse(resp.data);
            if (projects && cJSON_IsArray(projects)) {
                int pcount = cJSON_GetArraySize(projects);
                for (int p = 0; p < pcount; p++) {
                    cJSON *proj = cJSON_GetArrayItem(projects, p);
                    const char *pid = json_str(proj, "id");
                    const char *title = json_str(proj, "title");
                    const char *slug = json_str(proj, "slug");
                    if (!pid) continue;

                    for (int i = 0; i < count; i++) {
                        if (!entries[i].identified) continue;
                        if (strcmp(entries[i].project_id, pid) != 0) continue;

                        if (title && title[0])
                            copy_str(entries[i].mod->name,
                                     sizeof(entries[i].mod->name), title);
                        if (slug && slug[0])
                            snprintf(entries[i].mod->project_url,
                                     sizeof(entries[i].mod->project_url),
                                     "https://modrinth.com/mod/%s", slug);
                    }
                }
            }
            if (projects) cJSON_Delete(projects);
        }
        free(resp.data);
        g_string_free(url, TRUE);
    }

    g_list_free(keys);
    g_hash_table_destroy(seen);
}

/* ─── 对一组 hash 调用 /version_files/update ─── */
static void query_updates(ResolveEntry *entries, const int *idxs, int idx_count,
                          const char *loader, const char *mc_version)
{
    for (int start = 0; start < idx_count; start += BULK_CHUNK) {
        int end = start + BULK_CHUNK;
        if (end > idx_count) end = idx_count;

        cJSON *body = cJSON_CreateObject();
        cJSON *hashes = cJSON_CreateArray();
        cJSON_AddItemToObject(body, "hashes", hashes);
        for (int i = start; i < end; i++)
            cJSON_AddItemToArray(hashes, cJSON_CreateString(entries[idxs[i]].hash));
        cJSON_AddStringToObject(body, "algorithm", "sha1");

        if (loader && loader[0]) {
            cJSON *lds = cJSON_CreateArray();
            cJSON_AddItemToArray(lds, cJSON_CreateString(loader));
            cJSON_AddItemToObject(body, "loaders", lds);
        }
        if (mc_version && mc_version[0]) {
            cJSON *gvs = cJSON_CreateArray();
            cJSON_AddItemToArray(gvs, cJSON_CreateString(mc_version));
            cJSON_AddItemToObject(body, "game_versions", gvs);
        }

        cJSON *root = post_json("/version_files/update", body);
        cJSON_Delete(body);

        if (!root) continue;

        for (int i = start; i < end; i++) {
            ResolveEntry *e = &entries[idxs[i]];
            cJSON *version = cJSON_GetObjectItemCaseSensitive(root, e->hash);
            if (!version || !cJSON_IsObject(version)) continue;

            const char *id = json_str(version, "id");
            const char *number = json_str(version, "version_number");
            if (id) copy_str(e->latest_id, sizeof(e->latest_id), id);
            if (number) copy_str(e->latest_version, sizeof(e->latest_version), number);

            fill_file_info(version, e->mod->download_url,
                           sizeof(e->mod->download_url),
                           e->latest_sha1, sizeof(e->latest_sha1));

            /* 与已安装版本的 ID 比较，ID 相同即已是最新 */
            e->update_checked = true;
            if (e->local_id[0] && e->latest_id[0])
                e->has_update = (strcmp(e->local_id, e->latest_id) != 0);
            else if (e->local_version[0] && e->latest_version[0])
                e->has_update = (strcmp(e->local_version, e->latest_version) != 0);
        }

        cJSON_Delete(root);
    }
}

int modrinth_resolve_all(GPtrArray *mods, ModrinthProgressFunc progress,
                         gpointer userdata)
{
    if (!mods || mods->len == 0) return 0;

    guint total_mods = mods->len;
    ResolveEntry *entries = g_new0(ResolveEntry, total_mods);
    int count = 0;

    for (guint i = 0; i < total_mods; i++) {
        ModInfo *m = g_ptr_array_index(mods, i);
        if (!m || m->sha1[0] == '\0') continue;

        entries[count].mod = m;
        copy_str(entries[count].hash, sizeof(entries[count].hash), m->sha1);
        count++;
    }

    if (count == 0) {
        g_free(entries);
        return 0;
    }

    int total_steps = count * 2;
    int done = 0;

    /* ─── 第一步：识别本地文件 ─── */
    if (progress) progress(0, total_steps, "正在识别模组...", userdata);

    for (int start = 0; start < count; start += BULK_CHUNK) {
        int end = start + BULK_CHUNK;
        if (end > count) end = count;

        cJSON *body = cJSON_CreateObject();
        cJSON *hashes = cJSON_CreateArray();
        cJSON_AddItemToObject(body, "hashes", hashes);
        for (int i = start; i < end; i++)
            cJSON_AddItemToArray(hashes, cJSON_CreateString(entries[i].hash));
        cJSON_AddStringToObject(body, "algorithm", "sha1");

        cJSON *root = post_json("/version_files", body);
        cJSON_Delete(body);

        if (root) {
            for (int i = start; i < end; i++) {
                cJSON *version = cJSON_GetObjectItemCaseSensitive(root, entries[i].hash);
                if (version && cJSON_IsObject(version))
                    fill_identification(&entries[i], version);
            }
            cJSON_Delete(root);
        }

        done += end - start;
        if (progress) progress(done, total_steps, "正在识别模组...", userdata);
    }

    /* ─── 第二步：检测更新 ─── */
    char instance_mc[32];
    detect_instance_version(entries, count, instance_mc, sizeof(instance_mc));

    const char *loaders[MAX_LOADERS];
    int loader_count = collect_loaders(entries, count, loaders);

    if (loader_count == 0) {
        /* 没有任何加载器信息时退化为不限加载器 */
        int *idxs = g_new(int, count);
        int idx_count = 0;
        for (int i = 0; i < count; i++)
            if (entries[i].identified) idxs[idx_count++] = i;

        query_updates(entries, idxs, idx_count, NULL, instance_mc);
        g_free(idxs);
        done += count;
        if (progress) progress(done, total_steps, "正在检测更新...", userdata);
    } else {
        for (int l = 0; l < loader_count; l++) {
            int *idxs = g_new(int, count);
            int idx_count = 0;
            for (int i = 0; i < count; i++) {
                if (entries[i].identified
                    && strcmp(entries[i].loader, loaders[l]) == 0)
                    idxs[idx_count++] = i;
            }

            query_updates(entries, idxs, idx_count, loaders[l], instance_mc);

            done += idx_count;
            if (progress) progress(done, total_steps, "正在检测更新...", userdata);
            g_free(idxs);
        }
    }

    /* ─── 第三步：批量取项目名称 ─── */
    if (progress) progress(total_steps, total_steps, "正在获取模组信息...", userdata);
    fetch_project_names(entries, count);

    /* ─── 回写结果 ─── */
    int resolved = 0;
    for (int i = 0; i < count; i++) {
        ModInfo *m = entries[i].mod;

        if (!entries[i].identified) {
            m->status = MOD_STATUS_UNKNOWN;
            continue;
        }

        resolved++;
        m->source = MOD_SOURCE_MODRINTH;

        if (entries[i].project_id[0])
            copy_str(m->mod_id, sizeof(m->mod_id), entries[i].project_id);
        if (entries[i].local_version[0])
            copy_str(m->local_version, sizeof(m->local_version), entries[i].local_version);
        if (entries[i].loader[0])
            copy_str(m->loader, sizeof(m->loader), entries[i].loader);
        if (entries[i].mc_version[0])
            copy_str(m->mc_version, sizeof(m->mc_version), entries[i].mc_version);

        if (entries[i].has_update) {
            m->status = MOD_STATUS_UPDATE_AVAIL;
            copy_str(m->latest_version, sizeof(m->latest_version),
                     entries[i].latest_version);
            copy_str(m->latest_version_id, sizeof(m->latest_version_id),
                     entries[i].latest_id);
        } else if (entries[i].update_checked) {
            m->status = MOD_STATUS_UP_TO_DATE;
            m->latest_version[0] = '\0';
            m->latest_version_id[0] = '\0';
        } else {
            /* 更新查询失败时不谎报「最新」 */
            m->status = MOD_STATUS_UNKNOWN;
        }
    }

    g_free(entries);
    return resolved;
}
