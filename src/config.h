#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define CONFIG_FILE "config.json"
#define APP_NAME "Minecraft Mod Manager"
#define APP_VERSION "1.0.0"

/** 主题模式 */
typedef enum {
    THEME_SYSTEM = 0,   /* 跟随系统 */
    THEME_LIGHT  = 1,   /* 自定义浅色 */
    THEME_DARK   = 2,   /* 自定义深色 */
    THEME_PRESET_A  = 3, /* 预设A（暗红金） */
    THEME_PRESET_B  = 4, /* 预设B（森林绿） */
    THEME_PRESET_C  = 5, /* 预设C（海洋蓝） */
    THEME_PRESET_D  = 6, /* 预设D（暗夜紫） */
    THEME_COUNT
} ThemeMode;

typedef struct {
    char mod_dir[1024];          // 模组目录路径
    char backup_dir[1024];       // 备份目录路径
    int max_backups;             // 最大备份数（0=不限制）
    char curseforge_api_key[256];// CurseForge API Key
    bool auto_scan_on_start;     // 启动时自动扫描
    bool backup_before_update;   // 更新前自动备份
    ThemeMode theme;             // 主题模式
} AppConfig;

/**
 * 获取主题名称（中文，用于UI显示）
 */
const char *theme_get_name(ThemeMode theme);

/**
 * 应用主题 CSS 到全局
 */
void theme_apply(ThemeMode theme);

/**
 * 加载配置。若不存在则创建默认配置并保存。
 * @param config 输出配置
 * @return 0成功，-1失败
 */
int config_load(AppConfig *config);

/**
 * 保存配置到文件
 * @param config 要保存的配置
 * @return 0成功，-1失败
 */
int config_save(const AppConfig *config);

/**
 * 设置默认配置值
 * @param config 要初始化的配置
 */
void config_set_defaults(AppConfig *config);

#endif /* CONFIG_H */
