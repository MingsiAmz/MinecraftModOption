#ifndef HASH_H
#define HASH_H

#include <stdbool.h>
#include <stddef.h>

/**
 * 计算文件的 SHA1 哈希值
 * @param filepath 文件路径
 * @param output   输出缓冲区（至少 41 字节，接收 40 位十六进制字符串）
 * @return 0成功，-1失败
 */
int hash_sha1_file(const char *filepath, char *output);

/**
 * 计算缓冲区 SHA1 哈希值
 * @param data   数据缓冲区
 * @param len    数据长度
 * @param output 输出缓冲区（至少 41 字节）
 * @return 0成功，-1失败
 */
int hash_sha1_buffer(const unsigned char *data, size_t len, char *output);

#endif /* HASH_H */
