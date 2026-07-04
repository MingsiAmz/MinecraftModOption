#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stddef.h>

// 回调写入内存缓冲区
typedef struct {
    char *data;
    size_t len;
} MemoryBuffer;

/**
 * HTTP GET 请求
 * @param url      请求 URL
 * @param headers  额外的 HTTP 头（每行一个，NULL 结尾的数组），可为 NULL
 * @param output   输出内存缓冲区
 * @return 0成功，-1失败
 */
int network_get(const char *url, const char **headers, MemoryBuffer *output);

/**
 * 下载文件到本地
 * @param url      下载 URL
 * @param filepath 保存路径
 * @return 0成功，-1失败
 */
int network_download(const char *url, const char *filepath);

/**
 * 初始化网络模块（全局 curl init）
 * @return 0成功
 */
int network_init(void);

/**
 * 清理网络模块
 */
void network_cleanup(void);

#endif /* NETWORK_H */
