#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <cjson/cJSON.h>
#include <windows.h>
#include <shellapi.h>

// 全局临时目录缓冲区（用于 fallback 解压）
static char tmp_dir_global[MAX_PATH] = "";

// ─── ZIP 文件结构常量 ───
#define ZIP_LOCAL_FILE_HEADER_SIG  0x04034b50
#define ZIP_CENTRAL_DIR_SIG       0x02014b50
#define ZIP_END_CENTRAL_DIR_SIG   0x06054b50

#pragma pack(push, 1)
typedef struct {
    unsigned short version_needed;
    unsigned short flags;
    unsigned short compression;
    unsigned short mod_time;
    unsigned short mod_date;
    unsigned int   crc32;
    unsigned int   compressed_size;
    unsigned int   uncompressed_size;
    unsigned short filename_len;
    unsigned short extra_len;
} ZipLocalFileHeaderFooter;
#pragma pack(pop)

// ─── 从 ZIP/JAR 中读取文件内容（纯 Windows API，无外部依赖） ───
// 通过直接解析 ZIP 格式定位文件，使用 inflate 解压（如有压缩）
// 对于 Java JAR 中未压缩的条目（STORED），直接读取
static int read_file_from_jar(const char *jar_path, const char *internal_path,
                              char **output, size_t *output_len)
{
    *output = NULL;
    *output_len = 0;

    HANDLE hFile = CreateFileA(jar_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return -1;

    DWORD fsize = GetFileSize(hFile, NULL);
    if (fsize == INVALID_FILE_SIZE || fsize < 22) {
        CloseHandle(hFile);
        return -1;
    }

    // 将 ZIP 文件映射到内存
    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) { CloseHandle(hFile); return -1; }

    const unsigned char *data = (const unsigned char *)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!data) { CloseHandle(hMap); CloseHandle(hFile); return -1; }

    int result = -1;

    // 从文件末尾查找 End of Central Directory 记录
    // 用于找到 Central Directory 的偏移量
    const unsigned char *end_hdr = NULL;
    for (DWORD off = (fsize < 65557 ? 0 : fsize - 65557); off < fsize - 22; off++) {
        if (*(unsigned int *)(data + off) == ZIP_END_CENTRAL_DIR_SIG) {
            end_hdr = data + off;
            break;
        }
    }

    if (!end_hdr) goto cleanup;

    unsigned short num_entries = *(unsigned short *)(end_hdr + 10);
    unsigned int central_dir_offset = *(unsigned int *)(end_hdr + 16);
    (void)num_entries;

    // 标准化要查找的内部路径（统一用 '/'）
    char search_path[512];
    snprintf(search_path, sizeof(search_path), "%s", internal_path);
    for (char *p = search_path; *p; p++) if (*p == '\\') *p = '/';

    // 遍历 Central Directory 查找文件
    const unsigned char *cd = data + central_dir_offset;
    unsigned int cd_size = (unsigned int)(end_hdr - cd);

    const unsigned char *cd_end = cd + cd_size;
    while (cd + 46 <= cd_end) {
        unsigned int sig = *(unsigned int *)cd;
        if (sig != ZIP_CENTRAL_DIR_SIG) { cd++; continue; }

        unsigned short compression = *(unsigned short *)(cd + 10);
        unsigned int compressed_size = *(unsigned int *)(cd + 20);
        unsigned int uncompressed_size = *(unsigned int *)(cd + 24);
        unsigned short filename_len = *(unsigned short *)(cd + 28);
        unsigned short extra_len = *(unsigned short *)(cd + 30);
        unsigned int local_offset = *(unsigned int *)(cd + 42);

        // 提取文件名（全小写比较）
        if (filename_len > 0 && ((const unsigned char *)(cd + 46 + filename_len)) <= (data + fsize)) {
            // 构建文件名
            char entry_name[512];
            unsigned int name_len = filename_len;
            if (name_len > sizeof(entry_name) - 1) name_len = sizeof(entry_name) - 1;
            memcpy(entry_name, cd + 46, name_len);
            entry_name[name_len] = '\0';
            for (char *p = entry_name; *p; p++) if (*p == '\\') *p = '/';

            // 不区分大小写比较
            int cmp = 0;
            const char *a = entry_name, *b = search_path;
            while (*a && *b) {
                char ca = *a; if (ca >= 'A' && ca <= 'Z') ca += 32;
                char cb = *b; if (cb >= 'A' && cb <= 'Z') cb += 32;
                if (ca != cb) { cmp = (int)ca - (int)cb; break; }
                a++; b++;
            }
            if (!cmp && *a == *b) {
                // 找到文件！定位到 Local File Header 读取数据
                if (local_offset + 30 + filename_len + extra_len + compressed_size <= fsize) {
                    const unsigned char *lfh = data + local_offset;
                    unsigned short lf_flags = *(unsigned short *)(lfh + 6);
                    unsigned int lf_compressed = *(unsigned int *)(lfh + 18);
                    unsigned short lf_fname_len = *(unsigned short *)(lfh + 26);
                    unsigned short lf_extra_len = *(unsigned short *)(lfh + 28);
                    unsigned int data_offset = local_offset + 30 + lf_fname_len + lf_extra_len;

                    // 只处理 STORE（未压缩）的条目
                    // Java JAR 通常大部分条目是 STORED
                    if (compression == 0) {
                        unsigned int read_size = lf_compressed > 0 ? lf_compressed : uncompressed_size;
                        if (data_offset + read_size <= fsize) {
                            *output = malloc(read_size + 1);
                            if (*output) {
                                memcpy(*output, data + data_offset, read_size);
                                (*output)[read_size] = '\0';
                                *output_len = read_size;
                                result = 0;
                            }
                        }
                    } else if (compression == 8) {
                        // DEFLATE 压缩 — 使用 Windows 的 inflate
                        // 先尝试用 PowerShell 解压（兼容性更好）
                        // 这里 fallback 到 powersehll 方法
                        char ps_cmd[8192];
                        char win_path[MAX_PATH];
                        snprintf(win_path, sizeof(win_path), "%s", jar_path);
                        char clean_path[MAX_PATH];
                        snprintf(clean_path, sizeof(clean_path), "%s", search_path);

                        // PowerShell 脚本：展开指定文件
                        snprintf(ps_cmd, sizeof(ps_cmd),
                            "powershell -NoProfile -Command  "
                            "\"$zip = [System.IO.Compression.ZipFile]::OpenRead('%s'); "
                            "$entry = $zip.Entries | Where-Object { $_.FullName -eq '%s' }; "
                            "if ($entry) { "
                            "  $sr = New-Object System.IO.StreamReader($entry.Open()); "
                            "  $sr.ReadToEnd() "
                            "} "
                            "$zip.Dispose()\" 2>nul",
                            jar_path, search_path);

                        // 使用管道读取输出
                        FILE *ps = _popen(ps_cmd, "r");
                        if (ps) {
                            char buf[4096];
                            size_t total = 0, cap = 4096;
                            *output = malloc(cap);
                            if (*output) {
                                while (fgets(buf, sizeof(buf), ps)) {
                                    size_t blen = strlen(buf);
                                    if (total + blen + 1 > cap) {
                                        cap *= 2;
                                        char *newp = realloc(*output, cap);
                                        if (!newp) { free(*output); *output = NULL; break; }
                                        *output = newp;
                                    }
                                    memcpy(*output + total, buf, blen);
                                    total += blen;
                                }
                                if (*output) {
                                    (*output)[total] = '\0';
                                    *output_len = total;
                                    result = 0;
                                }
                            }
                            _pclose(ps);
                        } else {
                            // 最后的 fallback：用 Expand-Archive 解压到临时目录
                            char tmp_subdir[MAX_PATH];
                            GetTempPathA(MAX_PATH, tmp_dir_global);
                            snprintf(tmp_subdir, sizeof(tmp_subdir), "%smodmgr_%lu",
                                     tmp_dir_global, GetCurrentThreadId());
                            CreateDirectoryA(tmp_subdir, NULL);

                            snprintf(ps_cmd, sizeof(ps_cmd),
                                "powershell -NoProfile -Command "
                                "\"Expand-Archive -Path '%s' -DestinationPath '%s' -Force; "
                                "Get-Content -Path '%s\\\\%s' -Raw -Encoding UTF8\" 2>nul",
                                jar_path, tmp_subdir, tmp_subdir, search_path);

                            FILE *ps2 = _popen(ps_cmd, "r");
                            if (ps2) {
                                char buf[4096];
                                size_t total = 0, cap = 4096;
                                *output = malloc(cap);
                                if (*output) {
                                    while (fgets(buf, sizeof(buf), ps2)) {
                                        size_t blen = strlen(buf);
                                        if (total + blen + 1 > cap) {
                                            cap *= 2;
                                            char *newp = realloc(*output, cap);
                                            if (!newp) { free(*output); *output = NULL; break; }
                                            *output = newp;
                                        }
                                        memcpy(*output + total, buf, blen);
                                        total += blen;
                                    }
                                    if (*output) {
                                        (*output)[total] = '\0';
                                        *output_len = total;
                                        result = 0;
                                    }
                                }
                                _pclose(ps2);
                            }

                            // 清理临时目录
                            char rmdir_cmd[2048];
                            snprintf(rmdir_cmd, sizeof(rmdir_cmd),
                                     "rmdir /s /q \"%s\"", tmp_subdir);
                            system(rmdir_cmd);
                        }
                    }
                }
                break; // 找到即退出
            }
        }

        cd += 46 + filename_len + extra_len + *(unsigned short *)(cd + 32); // comment_len
    }

cleanup:
    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);

    if (result != 0 && *output) {
        free(*output);
        *output = NULL;
        *output_len = 0;
    }
    return result;
}

// ─── TOML 简单解析（只提取 key = value，支持 [header] 分组） ───
// 不引入完整 TOML 库，手动解析 mods.toml 关键字段
static char *toml_get_value(const char *toml_content, const char *section, const char *key)
{
    if (!toml_content) return NULL;

    const char *p = toml_content;
    int in_section = (section == NULL || section[0] == '\0');

    while (*p) {
        // 跳过行首空格
        while (*p == ' ' || *p == '\t') p++;

        // 注释
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        // Section header [xxx]
        if (*p == '[') {
            p++;
            char current_section[128];
            int i = 0;
            while (*p && *p != ']' && i < 127)
                current_section[i++] = *p++;
            current_section[i] = '\0';
            if (*p == ']') p++;
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;

            in_section = (section && strcmp(current_section, section) == 0);
            continue;
        }

        if (!in_section) {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        // key = value
        char current_key[128];
        int i = 0;
        while (*p && *p != '=' && *p != '\n' && i < 127)
            current_key[i++] = *p++;
        current_key[i] = '\0';

        // 去除尾部空格
        while (i > 0 && (current_key[i-1] == ' ' || current_key[i-1] == '\t'))
            current_key[--i] = '\0';

        if (*p != '=') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        p++; // 跳过 =

        // 跳过空格
        while (*p == ' ' || *p == '\t') p++;

        if (strcmp(current_key, key) == 0) {
            // 提取值
            char value[512];
            int vi = 0;
            if (*p == '"') {
                // 引号字符串
                p++;
                while (*p && *p != '"' && vi < 511)
                    value[vi++] = *p++;
            } else {
                // 普通值（到行尾或注释）
                while (*p && *p != '\n' && *p != '#' && vi < 511)
                    value[vi++] = *p++;
                // 去除尾部空格
                while (vi > 0 && (value[vi-1] == ' ' || value[vi-1] == '\t'))
                    vi--;
            }
            value[vi] = '\0';
            return strdup(value);
        }

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }

    return NULL;
}

// ─── 解析 mods.toml ───
int scanner_parse_mods_toml(const char *jar_path, ModInfo *info)
{
    char *content = NULL;
    size_t len = 0;

    // 尝试多个路径
    const char *paths[] = {
        "META-INF/mods.toml",
        "META-INF\\mods.toml",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        if (read_file_from_jar(jar_path, paths[i], &content, &len) == 0)
            break;
    }

    if (!content) return -1;

    // [[mods]] section
    char *modid = toml_get_value(content, "mods", "modId");
    char *name = toml_get_value(content, "mods", "displayName");
    char *version = toml_get_value(content, "mods", "version");
    char *mc_version = toml_get_value(content, "mods", "minecraftVersion");

    if (!name) name = toml_get_value(content, "mods", "displayName");
    if (!name) name = toml_get_value(content, "mods", "name");

    if (modid) {
        snprintf(info->mod_id, sizeof(info->mod_id), "%s", modid);
        free(modid);
    }
    if (name) {
        snprintf(info->name, sizeof(info->name), "%s", name);
        free(name);
    } else {
        // 如果没找到 displayName，用 modId 当名字
        if (info->mod_id[0]) snprintf(info->name, sizeof(info->name), "%s", info->mod_id);
    }
    if (version) {
        snprintf(info->local_version, sizeof(info->local_version), "%s", version);
        free(version);
    }
    if (mc_version) {
        snprintf(info->mc_version, sizeof(info->mc_version), "%s", mc_version);
        free(mc_version);
    }

    // 检测 loader 类型：mods.toml 多为 Forge / NeoForge
    char *loader = toml_get_value(content, "mods", "modLoader");
    if (loader) {
        if (strstr(loader, "neoforge") || strstr(loader, "NeoForge"))
            snprintf(info->loader, sizeof(info->loader), "NeoForge");
        else if (strstr(loader, "forge") || strstr(loader, "Forge"))
            snprintf(info->loader, sizeof(info->loader), "Forge");
        else
            snprintf(info->loader, sizeof(info->loader), "%s", loader);
        free(loader);
    }

    // 尝试读取描述
    char *desc = toml_get_value(content, "mods", "description");
    if (desc) {
        snprintf(info->description, sizeof(info->description), "%s", desc);
        free(desc);
    }

    free(content);
    return (info->mod_id[0] != '\0') ? 0 : -1;
}

// ─── 解析 fabric.mod.json ───
int scanner_parse_fabric_json(const char *jar_path, ModInfo *info)
{
    char *content = NULL;
    size_t len = 0;

    if (read_file_from_jar(jar_path, "fabric.mod.json", &content, &len) != 0)
        return -1;

    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root) return -1;

    cJSON *val;

    // id
    if ((val = cJSON_GetObjectItem(root, "id")) && cJSON_IsString(val))
        snprintf(info->mod_id, sizeof(info->mod_id), "%s", val->valuestring);

    // name
    if ((val = cJSON_GetObjectItem(root, "name")) && cJSON_IsString(val))
        snprintf(info->name, sizeof(info->name), "%s", val->valuestring);

    // version
    if ((val = cJSON_GetObjectItem(root, "version")) && cJSON_IsString(val))
        snprintf(info->local_version, sizeof(info->local_version), "%s", val->valuestring);

    // description
    if ((val = cJSON_GetObjectItem(root, "description")) && cJSON_IsString(val))
        snprintf(info->description, sizeof(info->description), "%s", val->valuestring);

    // 从 depends 找 minecraft 版本
    cJSON *depends = cJSON_GetObjectItem(root, "depends");
    if (depends && cJSON_IsObject(depends)) {
        cJSON *mc = cJSON_GetObjectItem(depends, "minecraft");
        if (mc && cJSON_IsString(mc))
            snprintf(info->mc_version, sizeof(info->mc_version), "%s", mc->valuestring);
    }

    // loader 类型：从 fabric.mod.json 可知是 Fabric
    if (info->loader[0] == '\0')
        snprintf(info->loader, sizeof(info->loader), "Fabric");

    // 检查是否是 Quilt（有 quilt_loader 字段）
    cJSON *quilt = cJSON_GetObjectItem(root, "quilt_loader");
    if (quilt) {
        snprintf(info->loader, sizeof(info->loader), "Quilt");
    }

    cJSON_Delete(root);
    return (info->mod_id[0] != '\0') ? 0 : -1;
}

// ─── 解析 mcmod.info（旧版） ───
int scanner_parse_mcmod_info(const char *jar_path, ModInfo *info)
{
    char *content = NULL;
    size_t len = 0;

    if (read_file_from_jar(jar_path, "mcmod.info", &content, &len) != 0)
        return -1;

    // mcmod.info 是一个 JSON 数组
    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return -1;
    }

    // 取第一个条目
    cJSON *first = cJSON_GetArrayItem(root, 0);
    if (!first) { cJSON_Delete(root); return -1; }

    cJSON *val;
    if ((val = cJSON_GetObjectItem(first, "modid")) && cJSON_IsString(val))
        snprintf(info->mod_id, sizeof(info->mod_id), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(first, "name")) && cJSON_IsString(val))
        snprintf(info->name, sizeof(info->name), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(first, "version")) && cJSON_IsString(val))
        snprintf(info->local_version, sizeof(info->local_version), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(first, "description")) && cJSON_IsString(val))
        snprintf(info->description, sizeof(info->description), "%s", val->valuestring);
    if ((val = cJSON_GetObjectItem(first, "mcversion")) && cJSON_IsString(val))
        snprintf(info->mc_version, sizeof(info->mc_version), "%s", val->valuestring);

    // mcmod.info 通常是 Forge 模组
    if (info->loader[0] == '\0')
        snprintf(info->loader, sizeof(info->loader), "Forge");

    cJSON_Delete(root);
    return (info->mod_id[0] != '\0') ? 0 : -1;
}

// ─── 解析单个 jar ───
int scanner_parse_jar(const char *filepath, ModInfo *info)
{
    memset(info, 0, sizeof(ModInfo));

    // 提取文件名
    const char *fname = strrchr(filepath, '\\');
    if (!fname) fname = strrchr(filepath, '/');
    if (!fname) fname = filepath;
    else fname++;
    snprintf(info->file_name, sizeof(info->file_name), "%s", fname);
    snprintf(info->file_path, sizeof(info->file_path), "%s", filepath);

    // 文件大小
    struct stat st;
    if (stat(filepath, &st) == 0)
        info->file_size = (long)st.st_size;

    // 尝试三种元数据格式
    if (scanner_parse_fabric_json(filepath, info) == 0)
        return 0;
    if (scanner_parse_mods_toml(filepath, info) == 0)
        return 0;
    if (scanner_parse_mcmod_info(filepath, info) == 0)
        return 0;

    // 都失败：使用文件名作为模组名
    // 移除 .jar 后缀
    char name_only[256];
    snprintf(name_only, sizeof(name_only), "%s", info->file_name);
    char *dot = strrchr(name_only, '.');
    if (dot) *dot = '\0';
    snprintf(info->name, sizeof(info->name), "%s", name_only);
    snprintf(info->mod_id, sizeof(info->mod_id), "%s", name_only);
    info->status = MOD_STATUS_UNKNOWN;

    return 0;
}

// ─── 扫描目录 ───
int scanner_scan_directory(const char *mod_dir, GPtrArray *mods)
{
    DIR *dir = opendir(mod_dir);
    if (!dir) return -1;

    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // 只处理 .jar 文件
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len < 4 || strcasecmp(name + len - 4, ".jar") != 0)
            continue;

        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s\\%s", mod_dir, name);

        ModInfo info;
        if (scanner_parse_jar(full_path, &info) == 0) {
            mod_list_add(&info);
            count++;
        }
    }

    closedir(dir);
    return count;
}
