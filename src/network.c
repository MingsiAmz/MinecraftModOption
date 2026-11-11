#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <glib.h>
#include <sys/stat.h>
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

/* Modrinth 的 CDN 对单条连接限速，实测单连接约 90 KB/s，而同时开 4 条
 * 能跑到 560 KB/s，可见瓶颈在「每条连接」而不是总带宽。所以大文件改成
 * 分段并发下载：先探测大小与 Range 支持，再按区间各开一条连接下到临时
 * 分片，最后按顺序拼接。任何一步失败都退回单连接，保证不会写出半个 jar。
 * 分段数由配置项 download_segments 决定（1..128），上限见 SEG_MAX_PARTS。 */
#define SEG_MIN_BYTES   (2L * 1024 * 1024)  /* 小于 2MB 不值得分段 */
#define SEG_MAX_PARTS   128                 /* 上限，与配置项范围一致 */
#define SEG_MIN_PART    (256L * 1024)       /* 每段至少 256KB，避免切得太碎 */
#define SEG_COPY_BUF    (64 * 1024)

typedef struct {
    const char *url;
    const char *ca_bundle;
    long long start;
    long long end;              /* 闭区间 */
    char path[MAX_PATH];
    int failed;
} SegmentTask;

typedef struct {
    long long size;
    int supports_ranges;
} ProbeInfo;

static size_t probe_header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
    ProbeInfo *info = (ProbeInfo *)userdata;
    size_t len = size * nitems;

    if (len > 16 && _strnicmp(buffer, "Accept-Ranges:", 14) == 0) {
        if (strstr(buffer, "bytes")) info->supports_ranges = 1;
    } else if (len > 15 && _strnicmp(buffer, "Content-Length:", 15) == 0) {
        long long value = _strtoi64(buffer + 15, NULL, 10);
        if (value > 0) info->size = value;
    }
    return len;
}

/* 用 HEAD 拿文件大小和 Range 支持情况 */
static int probe_remote(const char *url, const char *ca_bundle, ProbeInfo *info)
{
    memset(info, 0, sizeof(*info));

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, probe_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, info);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MinecraftModManager/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    if (ca_bundle) curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && info->size > 0) ? 0 : -1;
}

static void apply_transfer_options(CURL *curl, const char *ca_bundle)
{
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MinecraftModManager/1.0");
    /* 不设 CURLOPT_TIMEOUT：大模组按百来 KB/s 要几分钟，总时长限制会把
     * 正常下载判成失败。只限连接与持续低速。 */
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    /* HTTP 404 之类不能当成功：否则错误页面会被当成 jar 写进模组目录 */
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (ca_bundle) curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle);
}

static int download_single(const char *url, const char *filepath, const char *ca_bundle)
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
    apply_transfer_options(curl, ca_bundle);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    CURLcode res = curl_easy_perform(curl);

    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        remove(filepath);
        return -1;
    }
    return 0;
}

static gpointer segment_worker(gpointer userdata)
{
    SegmentTask *task = (SegmentTask *)userdata;

    CURL *curl = curl_easy_init();
    if (!curl) {
        task->failed = 1;
        return NULL;
    }

    FILE *f = fopen(task->path, "wb");
    if (!f) {
        curl_easy_cleanup(curl);
        task->failed = 1;
        return NULL;
    }

    char range[64];
    snprintf(range, sizeof(range), "%lld-%lld", task->start, task->end);

    curl_easy_setopt(curl, CURLOPT_URL, task->url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    apply_transfer_options(curl, task->ca_bundle);

    CURLcode res = curl_easy_perform(curl);

    fclose(f);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) task->failed = 1;
    return NULL;
}

/* 把分片按顺序拼接到目标文件 */
static int concat_segments(SegmentTask *tasks, int parts, const char *dest)
{
    FILE *out = fopen(dest, "wb");
    if (!out) return -1;

    char *buf = malloc(SEG_COPY_BUF);
    if (!buf) {
        fclose(out);
        return -1;
    }

    int rc = 0;
    for (int i = 0; i < parts && rc == 0; i++) {
        FILE *in = fopen(tasks[i].path, "rb");
        if (!in) { rc = -1; break; }

        size_t n;
        while ((n = fread(buf, 1, SEG_COPY_BUF, in)) > 0) {
            if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
        }
        fclose(in);
    }

    free(buf);
    fclose(out);
    return rc;
}

static int download_segmented(const char *url, const char *filepath,
                              const char *ca_bundle, long long size, int segments)
{
    int parts = segments;
    if (parts > SEG_MAX_PARTS) parts = SEG_MAX_PARTS;
    if (parts < 2) return -1;
    /* 段数不能多到每段只剩几十 KB，那样反而更慢 */
    if (size / parts < SEG_MIN_PART) {
        parts = (int)(size / SEG_MIN_PART);
        if (parts < 2) return -1;
    }
    if (parts > SEG_MAX_PARTS) parts = SEG_MAX_PARTS;

    SegmentTask *tasks = g_new0(SegmentTask, parts);
    GThread **threads = g_new(GThread *, parts);

    long long chunk = size / parts;
    for (int i = 0; i < parts; i++) {
        tasks[i].url = url;
        tasks[i].ca_bundle = ca_bundle;
        tasks[i].start = (long long)i * chunk;
        tasks[i].end = (i == parts - 1) ? (size - 1) : (tasks[i].start + chunk - 1);
        snprintf(tasks[i].path, sizeof(tasks[i].path), "%s.part%d", filepath, i);
    }

    for (int i = 0; i < parts; i++)
        threads[i] = g_thread_new("dl-seg", segment_worker, &tasks[i]);
    for (int i = 0; i < parts; i++)
        g_thread_join(threads[i]);
    g_free(threads);

    int failed = 0;
    for (int i = 0; i < parts; i++) {
        if (tasks[i].failed) failed = 1;
    }

    int rc = failed ? -1 : concat_segments(tasks, parts, filepath);

    for (int i = 0; i < parts; i++) remove(tasks[i].path);
    g_free(tasks);

    if (rc != 0) return -1;

    /* 拼接结果必须与远端大小一致，否则当作失败走单连接重下 */
    struct _stati64 st;
    if (_stati64(filepath, &st) != 0 || (long long)st.st_size != size) {
        remove(filepath);
        return -1;
    }
    return 0;
}

int network_download(const char *url, const char *filepath, int segments)
{
    const char *ca_bundle = portable_ca_bundle();

    if (segments < 1) segments = 1;

    if (segments > 1) {
        ProbeInfo info;
        if (probe_remote(url, ca_bundle, &info) == 0 &&
            info.supports_ranges && info.size >= SEG_MIN_BYTES) {
            if (download_segmented(url, filepath, ca_bundle, info.size, segments) == 0)
                return 0;
        }
    }

    return download_single(url, filepath, ca_bundle);
}
