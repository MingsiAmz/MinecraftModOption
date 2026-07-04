#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#define CONFIG_FILE "config.json"
#define APP_NAME "Minecraft Mod Manager"
#define APP_VERSION "1.0.0"

typedef struct {
    char mod_dir[1024];          // 模组目录路径
    char backup_dir[1024];       // 备份目录路径
    int max_backups;             // 最大备份数（0=不限制）
    char curseforge_api_key[256];// CurseForge API Key
    bool auto_scan_on_start;     // 启动时自动扫描
    bool backup_before_update;   // 更新前自动备份
} AppConfig;

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
