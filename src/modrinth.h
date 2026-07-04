#ifndef MODRINTH_H
#define MODRINTH_H

#include "mod_list.h"

#define MODRINTH_API_BASE "https://api.modrinth.com/v2"

/**
 * 通过模组 ID 查询 Modrinth 上模组的最新版本
 * @param mod           模组信息（需已设置 mod_id）
 * @param mc_version    目标 Minecraft 版本（如 "1.21"，NULL 表示任意）
 * @param loader        目标加载器（如 "fabric"，NULL 表示任意）
 * @return 0成功，-1失败（未找到/网络错误）
 */
int modrinth_query_version(ModInfo *mod, const char *mc_version, const char *loader);

/**
 * 使用项目 slug（URL 中的短名）搜索模组
 * @param slug 项目短名
 * @param mod  输出：模组信息更新
 * @return 0成功，-1失败
 */
int modrinth_search_slug(const char *slug, ModInfo *mod);

#endif /* MODRINTH_H */
