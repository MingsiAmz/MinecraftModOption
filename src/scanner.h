#ifndef SCANNER_H
#define SCANNER_H

#include "mod_list.h"

/**
 * 扫描指定目录中的所有 .jar 模组文件
 * 自动解析每个 jar 中的元数据（mods.toml / fabric.mod.json）
 * @param mod_dir 模组目录路径
 * @param mods    输出：模组列表（追加到已有列表）
 * @return 扫描到的模组数量，-1失败
 */
int scanner_scan_directory(const char *mod_dir, GPtrArray *mods);

/**
 * 解析单个 .jar 文件，提取模组元数据
 * @param filepath jar 文件完整路径
 * @param info     输出：模组信息
 * @return 0成功，-1失败
 */
int scanner_parse_jar(const char *filepath, ModInfo *info);

/**
 * 从 jar 中读取并解析 META-INF/mods.toml（Forge / NeoForge）
 * @param jar_path jar 文件路径
 * @param info     输出：模组信息
 * @return 0找到，-1未找到
 */
int scanner_parse_mods_toml(const char *jar_path, ModInfo *info);

/**
 * 从 jar 中读取并解析 fabric.mod.json（Fabric / Quilt）
 * @param jar_path jar 文件路径
 * @param info     输出：模组信息
 * @return 0找到，-1未找到
 */
int scanner_parse_fabric_json(const char *jar_path, ModInfo *info);

/**
 * 从 jar 中读取 mcmod.info（旧版 Forge）
 * @param jar_path jar 文件路径
 * @param info     输出：模组信息
 * @return 0找到，-1未找到
 */
int scanner_parse_mcmod_info(const char *jar_path, ModInfo *info);

#endif /* SCANNER_H */
