#ifndef MOD_LIST_H
#define MOD_LIST_H

#include <gtk/gtk.h>
#include <stdbool.h>

#define MOD_STATUS_UP_TO_DATE    0  // 最新
#define MOD_STATUS_UPDATE_AVAIL  1  // 可更新
#define MOD_STATUS_UNKNOWN       2  // 未匹配到远程版本
#define MOD_STATUS_ERROR         3  // 解析错误

#define MOD_SOURCE_UNKNOWN    0     // 未知来源
#define MOD_SOURCE_CURSEFORGE 1     // CurseForge
#define MOD_SOURCE_MODRINTH   2     // Modrinth

typedef struct {
    char mod_id[128];           // 模组唯一标识
    char name[256];             // 模组名称
    char local_version[64];     // 本地版本
    char latest_version[64];    // 最新版本（远程）
    char mc_version[32];        // 支持的MC版本
    char loader[32];            // ModLoader (Fabric/Forge/NeoForge/Quilt)
    int source;                 // 来源平台
    int status;                 // 状态
    char file_name[256];        // 文件名
    char file_path[1024];       // 完整路径
    long file_size;             // 文件大小（字节）
    char project_url[512];      // 项目主页
    char description[1024];     // 模组描述
    int backup_count;           // 备份数量
} ModInfo;

/* ─── 列表列定义 ─── */
enum {
    COL_CHECK = 0,
    COL_NAME,
    COL_LOCAL_VER,
    COL_LATEST_VER,
    COL_SOURCE,
    COL_STATUS,
    COL_SIZE,
    COL_MOD_PTR,   // 存储 ModInfo 指针
    NUM_COLS
};

/**
 * 初始化模组列表模块（数据层）
 */
void mod_list_init(void);

/**
 * 获取单例模式下的模组列表
 * @return 存储 ModInfo* 的 GPtrArray
 */
GPtrArray *mod_list_get_all(void);

/**
 * 向列表添加模组的深拷贝
 */
void mod_list_add(const ModInfo *info);

/**
 * 清空模组列表
 */
void mod_list_clear(void);

/**
 * 获取模组数量
 */
int mod_list_count(void);

/**
 * 根据索引获取模组
 * @return ModInfo 指针
 */
ModInfo *mod_list_get(int index);

/**
 * 获取可更新的模组数量
 */
int mod_list_update_count(void);

/**
 * 从列表中移除指定索引的模组（不删除文件）
 */
void mod_list_remove_at(int index);

/**
 * 释放模组列表
 */
void mod_list_free(void);

#endif /* MOD_LIST_H */
