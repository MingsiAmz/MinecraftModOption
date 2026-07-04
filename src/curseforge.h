#ifndef CURSEFORGE_H
#define CURSEFORGE_H

#include "mod_list.h"

#define CURSEFORGE_API_BASE "https://api.curseforge.com/v1"

/**
 * 通过模组 ID 查询 CurseForge 上模组的最新版本
 * @param mod        模组信息（需已设置 mod_id）
 * @param api_key    CurseForge API Key
 * @param mc_version 目标 Minecraft 版本
 * @return 0成功，-1失败
 */
int curseforge_query_version(ModInfo *mod, const char *api_key, const char *mc_version);

/**
 * 通过搜索名称查找 CurseForge 模组
 * @param name    模组名称
 * @param mod     输出：模组信息
 * @param api_key API Key
 * @return 0成功，-1失败
 */
int curseforge_search(const char *name, ModInfo *mod, const char *api_key);

#endif /* CURSEFORGE_H */
