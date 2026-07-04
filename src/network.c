#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// ─── libcurl 写回调 ───
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total = size * nmemb;
    MemoryBuffer *mem = (MemoryBuffer *)userp;

    char *ptr = realloc(mem->data, mem->len + total + 1);
    if (!ptr) return 0;

    mem->data = ptr;
    memcpy(&(mem->data[mem->len]), contents, total);
    mem->len += total;
    mem->data[mem->len] = '\0';
    return total;
}

// ─── libcurl 写文件回调 ───
static size_t write_file_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    FILE *f = (FILE *)userp;
    size_t written = fwrite(contents, size, nmemb, f);
    return written;
}

// ─── 下载进度回调（默认不处理，后续可用于进度条） ───
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow)
{
    (void)clientp; (void)ultotal; (void)ulnow;
    // 可在这里触发 GTK 进度更新
    return 0;
}

// ─── 初始化 ───
int network_init(void)
{
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    return (res == CURLE_OK) ? 0 : -1;
}

void network_cleanup(void)
{
    curl_global_cleanup();
}

// ─── HTTP GET ───
int network_get(const char *url, const char **headers, MemoryBuffer *output)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    output->data = malloc(1);
    output->data[0] = '\0';
    output->len = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)output);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MinecraftModManager/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    // 设置 HTTP 头
    struct curl_slist *chunk = NULL;
    if (headers) {
        for (int i = 0; headers[i] != NULL; i++) {
            chunk = curl_slist_append(chunk, headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    CURLcode res = curl_easy_perform(curl);

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

// ─── 下载文件 ───
int network_download(const char *url, const char *filepath)
{
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MinecraftModManager/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    CURLcode res = curl_easy_perform(curl);

    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(filepath); // 下载失败则清理
        return -1;
    }
    return 0;
}
