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

/**
 * 进度回调：done 已完成数量，total 总数，stage 当前阶段描述。
 */
typedef void (*ModrinthProgressFunc)(int done, int total, const char *stage,
                                     gpointer userdata);

/**
 * 用本地文件的 SHA1 批量识别模组并检测更新（Modrinth 批量接口）
 *
 * 内部流程：
 *   1. POST /version_files         —— SHA1 → 版本信息（识别模组）
 *   2. POST /version_files/update  —— SHA1 + 加载器 + MC 版本 → 最新版本
 *   3. GET  /projects?ids=[...]    —— 批量获取项目名称
 *
 * 结果直接写回 ModInfo（mod_id/local_version/latest_version/latest_version_id/
 * download_url/name/project_url/source/status）。需要 mod->sha1 已填充。
 *
 * @param mods      ModInfo* 数组
 * @param progress  进度回调，可为 NULL
 * @param userdata  传给进度回调的上下文
 * @return 成功识别的模组数量
 */
int modrinth_resolve_all(GPtrArray *mods, ModrinthProgressFunc progress,
                         gpointer userdata);

#endif /* MODRINTH_H */
