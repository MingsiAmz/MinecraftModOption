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

static void theme_apply_css(ThemeMode theme)
{
    /* 基础重置：所有主题通用 */
    const char *base =
        "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
        "  margin: 2px 3px; padding: 7px 16px; font-size: 13px;"
        "  border-radius: 6px; border-style: solid; border-width: 1px;"
        "  font-weight: normal; text-shadow: none; box-shadow: none;"
        "  background-image: none;"
        "}"
        "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
        "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
        "  background-image: none;"
        "}"
        "#search-entry {"
        "  border-radius: 16px; padding: 4px 12px;"
        "  min-height: 28px; font-size: 13px;"
        "}"
        "#mod-treeview { font-size: 13px; border: none; outline: none; }"
        "#mod-progress { min-height: 22px; border: none; margin: 4px 10px; }"
        "#mod-progress trough { border-radius: 4px; min-height: 18px; }"
        "#mod-progress progress { border-radius: 4px; min-height: 18px; }"
        "#status-bar { padding: 5px 10px; font-size: 12px; }";

    const char *theme_part;

    switch (theme) {
    case THEME_LIGHT:
        theme_part =
            "window { background-color: #f5f5f7; color: #000000; }"
            "#toolbar { background: #ffffff; border-bottom: 1px solid #e0e0e0; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #ffffff !important; color: #000000 !important;"
            "  border-color: #d0d0d0 !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #f0f0f0 !important; border-color: #bbbbbb !important;"
            "}"
            "#search-bar { background: #ffffff; padding: 6px 10px; }"
            "#search-entry { background: #f8f8f8; color: #000000; border-color: #ddd;"
            "  caret-color: #000000; }"
            "#search-entry:focus { border-color: #4a90d9; background: #fff; }"
            "#search-entry:placeholder { color: #999999; }"
            "#mod-treeview { background: #ffffff; color: #000000; }"
            "#mod-treeview:selected { background: #e3f0ff; color: #000000; }"
            "#mod-treeview:selected:focus { background: #cce5ff; color: #000000; }"
            "#status-bar { background: #ffffff; border-top: 1px solid #e0e0e0; color: #333333; }"
            "#mod-progress trough { background: #eeeeee; }"
            "#mod-progress progress { background: #4a90d9; }"
            "#mod-progress * { color: #000000; }";
        break;

    case THEME_DARK:
        theme_part =
            "window { background-color: #1e1e2e; color: #ffffff; }"
            "#toolbar { background: #252536; border-bottom: 1px solid #333350; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #2d2d44 !important; color: #ffffff !important;"
            "  border-color: #45456a !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #3d3d5c !important; border-color: #5a5a8a !important;"
            "}"
            "#search-bar { background: #252536; padding: 6px 10px; }"
            "#search-entry { background: #2d2d44; color: #ffffff; border-color: #45456a;"
            "  caret-color: #ffffff; }"
            "#search-entry:focus { border-color: #89b4fa; }"
            "#search-entry:placeholder { color: #6c7086; }"
            "#mod-treeview { background: #1e1e2e; color: #ffffff; }"
            "#mod-treeview:selected { background: #45456a; color: #ffffff; }"
            "#mod-treeview:selected:focus { background: #55557a; color: #ffffff; }"
            "#status-bar { background: #252536; border-top: 1px solid #333350; color: #cdd6f4; }"
            "#mod-progress trough { background: #333350; }"
            "#mod-progress progress { background: #89b4fa; }"
            "#mod-progress * { color: #ffffff; }";
        break;

    case THEME_PRESET_A:
        theme_part =
            "window { background-color: #1a0f0f; color: #e8c87a; }"
            "#toolbar { background: #2a1414; border-bottom: 1px solid #4a2020; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #3a1a1a !important; color: #e8c87a !important;"
            "  border-color: #6a3030 !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #4a2828 !important; border-color: #8a4040 !important;"
            "}"
            "#search-bar { background: #2a1414; padding: 6px 10px; }"
            "#search-entry { background: #3a1a1a; color: #e8c87a; border-color: #6a3030;"
            "  caret-color: #e8c87a; }"
            "#search-entry:focus { border-color: #e8c87a; }"
            "#search-entry:placeholder { color: #8a6040; }"
            "#mod-treeview { background: #1a0f0f; color: #e8c87a; }"
            "#mod-treeview:selected { background: #6a3030; color: #f0d89a; }"
            "#status-bar { background: #2a1414; border-top: 1px solid #4a2020; color: #c4a86a; }"
            "#mod-progress trough { background: #4a2020; }"
            "#mod-progress progress { background: #e8c87a; }"
            "#mod-progress * { color: #1a0f0f; }";
        break;

    case THEME_PRESET_B:
        theme_part =
            "window { background-color: #0f1a0f; color: #a8d8a0; }"
            "#toolbar { background: #162916; border-bottom: 1px solid #2a4a2a; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #1e3a1e !important; color: #a8d8a0 !important;"
            "  border-color: #3a6a3a !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #2a4a2a !important; border-color: #4a8a4a !important;"
            "}"
            "#search-bar { background: #162916; padding: 6px 10px; }"
            "#search-entry { background: #1e3a1e; color: #a8d8a0; border-color: #3a6a3a;"
            "  caret-color: #a8d8a0; }"
            "#search-entry:focus { border-color: #7acc70; }"
            "#search-entry:placeholder { color: #5a8a50; }"
            "#mod-treeview { background: #0f1a0f; color: #a8d8a0; }"
            "#mod-treeview:selected { background: #3a6a3a; color: #d0f0c8; }"
            "#status-bar { background: #162916; border-top: 1px solid #2a4a2a; color: #88b880; }"
            "#mod-progress trough { background: #2a4a2a; }"
            "#mod-progress progress { background: #5ab84a; }"
            "#mod-progress * { color: #0f1a0f; }";
        break;

    case THEME_PRESET_C:
        theme_part =
            "window { background-color: #0f1420; color: #88c0f0; }"
            "#toolbar { background: #1a2440; border-bottom: 1px solid #2a3a60; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #222d50 !important; color: #88c0f0 !important;"
            "  border-color: #3a4a70 !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #2a3a60 !important; border-color: #4a6a90 !important;"
            "}"
            "#search-bar { background: #1a2440; padding: 6px 10px; }"
            "#search-entry { background: #222d50; color: #88c0f0; border-color: #3a4a70;"
            "  caret-color: #88c0f0; }"
            "#search-entry:focus { border-color: #5aa0e0; }"
            "#search-entry:placeholder { color: #4a6a90; }"
            "#mod-treeview { background: #0f1420; color: #88c0f0; }"
            "#mod-treeview:selected { background: #3a4a70; color: #b0d8ff; }"
            "#status-bar { background: #1a2440; border-top: 1px solid #2a3a60; color: #70a0d0; }"
            "#mod-progress trough { background: #2a3a60; }"
            "#mod-progress progress { background: #3a7ad0; }"
            "#mod-progress * { color: #0f1420; }";
        break;

    case THEME_PRESET_D:
        theme_part =
            "window { background-color: #120f1a; color: #c8a8e0; }"
            "#toolbar { background: #201a30; border-bottom: 1px solid #382a50; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #2a2240 !important; color: #c8a8e0 !important;"
            "  border-color: #483a60 !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #382a50 !important; border-color: #685080 !important;"
            "}"
            "#search-bar { background: #201a30; padding: 6px 10px; }"
            "#search-entry { background: #2a2240; color: #c8a8e0; border-color: #483a60;"
            "  caret-color: #c8a8e0; }"
            "#search-entry:focus { border-color: #b080d8; }"
            "#search-entry:placeholder { color: #685080; }"
            "#mod-treeview { background: #120f1a; color: #c8a8e0; }"
            "#mod-treeview:selected { background: #483a60; color: #e0c8f8; }"
            "#status-bar { background: #201a30; border-top: 1px solid #382a50; color: #a888c8; }"
            "#mod-progress trough { background: #382a50; }"
            "#mod-progress progress { background: #9060c8; }"
            "#mod-progress * { color: #120f1a; }";
        break;

    default:
    case THEME_SYSTEM:
        theme_part =
            "window { background-color: #f5f5f7; color: #000000; }"
            "#toolbar { background: #ffffff; border-bottom: 1px solid #e0e0e0; padding: 8px; }"
            "#btn-scan, #btn-upd-sel, #btn-upd-all, #btn-roll, #btn-del, #btn-set {"
            "  background: #ffffff !important; color: #000000 !important;"
            "  border-color: #d0d0d0 !important;"
            "}"
            "#btn-scan:hover, #btn-upd-sel:hover, #btn-upd-all:hover,"
            "#btn-roll:hover, #btn-del:hover, #btn-set:hover {"
            "  background: #f0f0f0 !important; border-color: #bbbbbb !important;"
            "}"
            "#search-bar { background: #ffffff; padding: 6px 10px; }"
            "#search-entry { background: #f8f8f8; color: #000000; border-color: #ddd;"
            "  caret-color: #000000; }"
            "#search-entry:focus { border-color: #4a90d9; background: #fff; }"
            "#search-entry:placeholder { color: #999999; }"
            "#mod-treeview { background: #ffffff; color: #000000; }"
            "#mod-treeview:selected { background: #e3f0ff; color: #000000; }"
            "#mod-treeview:selected:focus { background: #cce5ff; color: #000000; }"
            "#status-bar { background: #ffffff; border-top: 1px solid #e0e0e0; color: #333333; }"
            "#mod-progress trough { background: #eeeeee; }"
            "#mod-progress progress { background: #4a90d9; }"
            "#mod-progress * { color: #000000; }";
        break;
    }

    /* 拼接完整 CSS */
    char *full_css = malloc(strlen(base) + strlen(theme_part) + 1);
    if (full_css) {
        strcpy(full_css, theme_part);
        strcat(full_css, base);

        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(provider, full_css);

        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        g_object_unref(provider);
        free(full_css);
    }
}

void theme_apply(ThemeMode theme)
{
    theme_apply_css(theme);
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
