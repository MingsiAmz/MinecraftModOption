#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* 便携版的 CA 证书：libcurl 的默认 CA 路径编进的是 MSYS2 安装前缀，
 * 免安装包是平铺布局，那个路径不存在，HTTPS 请求会以
 * "Problem with the SSL CA cert (path? access rights?)" 全部失败
 * （表现为模组全部「未匹配」、检测不到更新）。这里优先用 exe 同级的
 * cacert.pem；找不到时保持 libcurl 原有行为。 */
#ifdef _WIN32
static const char *portable_ca_bundle(void)
{
    static char path[MAX_PATH];
    static int resolved = 0;

    if (!resolved) {
        resolved = 1;

        char exe[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, exe, sizeof(exe));
        if (len > 0 && len < sizeof(exe)) {
            char *sep = strrchr(exe, '\\');
            if (sep) {
                *sep = '\0';
                int n = snprintf(path, sizeof(path), "%s\\cacert.pem", exe);
                if (n <= 0 || (size_t)n >= sizeof(path) ||
                    GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
                    path[0] = '\0';
                }
            }
        }
    }

    return path[0] ? path : NULL;
}
#else
static const char *portable_ca_bundle(void) { return NULL; }
#endif

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

// ─── 通用请求执行（post_body 为 NULL 时走 GET） ───
static int perform_request(const char *url, const char **headers,
                           const char *post_body, MemoryBuffer *output,
                           long *http_code)
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
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    const char *ca_bundle = portable_ca_bundle();
    if (ca_bundle)
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);

    if (post_body) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post_body));
    }

    // 设置 HTTP 头
    struct curl_slist *chunk = NULL;
    if (headers) {
        for (int i = 0; headers[i] != NULL; i++) {
            chunk = curl_slist_append(chunk, headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
        fprintf(stderr, "[network] %s 失败: %s\n", url, curl_easy_strerror(res));

    if (http_code) {
        long code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        *http_code = code;
    }

    if (chunk) curl_slist_free_all(chunk);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK) ? 0 : -1;
}

// ─── HTTP GET ───
int network_get(const char *url, const char **headers, MemoryBuffer *output)
{
    return perform_request(url, headers, NULL, output, NULL);
}

// ─── HTTP POST JSON ───
int network_post_json(const char *url, const char **headers, const char *json,
                      MemoryBuffer *output, long *http_code)
{
    return perform_request(url, headers, json, output, http_code);
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
    /* 不能用 CURLOPT_TIMEOUT 限制总时长：整合包的模组动辄二三十 MB，
     * 按百来 KB/s 算要几分钟，设总超时会把正常下载判成失败（表现就是
     * 「有时更新失败」）。这里只限制连接时间，并用低速阈值兜底：
     * 连续 60 秒低于 1 KB/s 才认为连接已经死了。 */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* HTTP 404 之类不能当成功：否则错误页面会被当成 jar 写进模组目录 */
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    const char *ca_bundle = portable_ca_bundle();
    if (ca_bundle)
        curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);

    CURLcode res = curl_easy_perform(curl);

    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(filepath); // 下载失败则清理
        return -1;
    }
    return 0;
}
