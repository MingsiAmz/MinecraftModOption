#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <gtk/gtk.h>

const char *theme_get_name(ThemeMode theme)
{
    static const char *names[] = {
        [THEME_SYSTEM]   = "🌗 跟随系统",
        [THEME_LIGHT]    = "☀️ 浅色模式",
        [THEME_DARK]     = "🌙 深色模式",
        [THEME_PRESET_A] = "🔴 暗红金",
        [THEME_PRESET_B] = "🌲 森林绿",
        [THEME_PRESET_C] = "🌊 海洋蓝",
        [THEME_PRESET_D] = "🟣 暗夜紫",
    };
    if (theme >= 0 && theme < THEME_COUNT)
        return names[theme];
    return "未知";
}

static const char *theme_css(ThemeMode theme)
{
    switch (theme) {
    case THEME_LIGHT:
        return
            "window { background-color: #f5f5f7; }"
            ".toolbar button { background: #ffffff; color: #333;"
            "  border: 1px solid #d0d0d0; }"
            ".toolbar button:hover { background: #f0f0f0; }"
            "treeview { font-size: 13px; }"
            "statusbar { background: #ffffff; border-top: 1px solid #e0e0e0; }"
            "progressbar trough { background: #eee; }"
            "progressbar progress { background: #4a90d9; }";

    case THEME_DARK:
        return
            "window { background-color: #1e1e2e; }"
            ".toolbar { background: #252536; border-bottom: 1px solid #333350; }"
            ".toolbar button { background: #2d2d44; color: #cdd6f4;"
            "  border: 1px solid #45456a; }"
            ".toolbar button:hover { background: #3d3d5c; }"
            ".search-bar { background: #252536; }"
            ".search-bar entry { background: #2d2d44; color: #cdd6f4;"
            "  border: 1px solid #45456a; }"
            ".search-bar entry:focus { border-color: #89b4fa; }"
            "treeview { font-size: 13px; background: #1e1e2e; color: #cdd6f4; }"
            "treeview:selected { background: #45456a; color: #cdd6f4; }"
            "statusbar { background: #252536; border-top: 1px solid #333350;"
            "  color: #a6adc8; }"
            "progressbar trough { background: #333350; }"
            "progressbar progress { background: #89b4fa; }";

    case THEME_PRESET_A:
        return
            "window { background-color: #1a0f0f; }"
            ".toolbar { background: #2a1414; border-bottom: 1px solid #4a2020; }"
            ".toolbar button { background: #3a1a1a; color: #e8c87a;"
            "  border: 1px solid #6a3030; }"
            ".toolbar button:hover { background: #4a2828; }"
            ".search-bar { background: #2a1414; }"
            ".search-bar entry { background: #3a1a1a; color: #e8c87a;"
            "  border: 1px solid #6a3030; }"
            ".search-bar entry:focus { border-color: #e8c87a; }"
            "treeview { font-size: 13px; background: #1a0f0f; color: #d4b87a; }"
            "treeview:selected { background: #6a3030; color: #f0d89a; }"
            "statusbar { background: #2a1414; border-top: 1px solid #4a2020;"
            "  color: #c4a86a; }"
            "progressbar trough { background: #4a2020; }"
            "progressbar progress { background: #e8c87a; }";

    case THEME_PRESET_B:
        return
            "window { background-color: #0f1a0f; }"
            ".toolbar { background: #162916; border-bottom: 1px solid #2a4a2a; }"
            ".toolbar button { background: #1e3a1e; color: #a8d8a0;"
            "  border: 1px solid #3a6a3a; }"
            ".toolbar button:hover { background: #2a4a2a; }"
            ".search-bar { background: #162916; }"
            ".search-bar entry { background: #1e3a1e; color: #a8d8a0;"
            "  border: 1px solid #3a6a3a; }"
            ".search-bar entry:focus { border-color: #7acc70; }"
            "treeview { font-size: 13px; background: #0f1a0f; color: #a8d8a0; }"
            "treeview:selected { background: #3a6a3a; color: #d0f0c8; }"
            "statusbar { background: #162916; border-top: 1px solid #2a4a2a;"
            "  color: #88b880; }"
            "progressbar trough { background: #2a4a2a; }"
            "progressbar progress { background: #5ab84a; }";

    case THEME_PRESET_C:
        return
            "window { background-color: #0f1420; }"
            ".toolbar { background: #1a2440; border-bottom: 1px solid #2a3a60; }"
            ".toolbar button { background: #222d50; color: #88c0f0;"
            "  border: 1px solid #3a4a70; }"
            ".toolbar button:hover { background: #2a3a60; }"
            ".search-bar { background: #1a2440; }"
            ".search-bar entry { background: #222d50; color: #88c0f0;"
            "  border: 1px solid #3a4a70; }"
            ".search-bar entry:focus { border-color: #5aa0e0; }"
            "treeview { font-size: 13px; background: #0f1420; color: #88c0f0; }"
            "treeview:selected { background: #3a4a70; color: #b0d8ff; }"
            "statusbar { background: #1a2440; border-top: 1px solid #2a3a60;"
            "  color: #70a0d0; }"
            "progressbar trough { background: #2a3a60; }"
            "progressbar progress { background: #3a7ad0; }";

    case THEME_PRESET_D:
        return
            "window { background-color: #120f1a; }"
            ".toolbar { background: #201a30; border-bottom: 1px solid #382a50; }"
            ".toolbar button { background: #2a2240; color: #c8a8e0;"
            "  border: 1px solid #483a60; }"
            ".toolbar button:hover { background: #382a50; }"
            ".search-bar { background: #201a30; }"
            ".search-bar entry { background: #2a2240; color: #c8a8e0;"
            "  border: 1px solid #483a60; }"
            ".search-bar entry:focus { border-color: #b080d8; }"
            "treeview { font-size: 13px; background: #120f1a; color: #c8a8e0; }"
            "treeview:selected { background: #483a60; color: #e0c8f8; }"
            "statusbar { background: #201a30; border-top: 1px solid #382a50;"
            "  color: #a888c8; }"
            "progressbar trough { background: #382a50; }"
            "progressbar progress { background: #9060c8; }";

    default:
    case THEME_SYSTEM:
        return
            "window { background-color: #f5f5f7; }"
            ".toolbar { background: #ffffff; border-bottom: 1px solid #e0e0e0; padding: 8px; }"
            ".toolbar button { margin: 2px 3px; padding: 7px 16px; font-size: 13px;"
            "   border-radius: 6px; border: 1px solid #d0d0d0;"
            "   background: #ffffff; color: #333; }"
            ".toolbar button:hover { background: #f0f0f0; border-color: #bbb; }"
            ".search-bar { background: #ffffff; padding: 6px 10px; }"
            ".search-bar entry { border-radius: 16px; padding: 4px 12px;"
            "   border: 1px solid #ddd; background: #f8f8f8;}"
            ".search-bar entry:focus { border-color: #4a90d9; background: #fff; }"
            "treeview { font-size: 13px; }"
            "treeview:selected { background: #e3f0ff; color: #1a1a1a; }"
            "statusbar { background: #ffffff; border-top: 1px solid #e0e0e0;"
            "   padding: 5px 10px; font-size: 12px; color: #555; }"
            "progressbar { min-height: 22px; }"
            "progressbar trough { border-radius: 4px; background: #eee; }"
            "progressbar progress { background: #4a90d9;"
            "   border-radius: 4px; }";
    }
}

void theme_apply(ThemeMode theme)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, theme_css(theme));

    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
}

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
    config->theme = THEME_SYSTEM;
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
    cJSON_AddNumberToObject(root, "theme", (int)config->theme);

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
    if ((val = cJSON_GetObjectItem(root, "theme")) && cJSON_IsNumber(val))
        config->theme = (ThemeMode)val->valueint;

    cJSON_Delete(root);
    ensure_backup_dir(config);
    return 0;
}
