#ifndef CACHE_H
#define CACHE_H

#include "mod_list.h"
#include <stdbool.h>

#define CACHE_FILE "mods_cache.json"

/**
 * 保存模组列表到缓存文件
 * @param mods 模组列表
 * @return 0成功，-1失败
 */
int cache_save(GPtrArray *mods);

/**
 * 从缓存文件加载模组列表
 * @param mods 输出：模组列表（会先清空再填充）
 * @return 加载的模组数量，-1失败
 */
int cache_load(GPtrArray *mods);

/**
 * 获取模组的远程版本信息（缓存）
 * @param mod_id 模组 ID
 * @param latest_version 输出：最新版本号
 * @param project_url 输出：项目 URL
 * @return 0找到，-1未找到
 */
int cache_get_remote_info(const char *mod_id, char *latest_version, char *project_url);

/**
 * 更新模组的远程版本信息到缓存
 */
void cache_update_remote(const char *mod_id, const char *latest_version, const char *project_url);

#endif /* CACHE_H */
