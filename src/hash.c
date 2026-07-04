#include "hash.h"
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

// 使用 Windows CryptoAPI (wincrypt.h) 实现 SHA1
// 无需额外依赖 OpenSSL

int hash_sha1_buffer(const unsigned char *data, size_t len, char *output)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD hash_len = 20;
    int ret = -1;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT))
        return -1;

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
        goto cleanup;

    if (!CryptHashData(hHash, data, (DWORD)len, 0))
        goto cleanup;

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0))
        goto cleanup;

    // 转为 40 位十六进制字符串
    for (DWORD i = 0; i < hash_len; i++) {
        sprintf(output + i * 2, "%02x", hash[i]);
    }
    output[40] = '\0';
    ret = 0;

cleanup:
    if (hHash) CryptDestroyHash(hHash);
    if (hProv) CryptReleaseContext(hProv, 0);
    return ret;
}

int hash_sha1_file(const char *filepath, char *output)
{
    FILE *f = fopen(filepath, "rb");
    if (!f) return -1;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE buffer[8192];
    BYTE hash[20];
    DWORD hash_len = 20;
    int ret = -1;

    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT))
        goto cleanup_file;

    if (!CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
        goto cleanup;

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (!CryptHashData(hHash, buffer, (DWORD)bytes_read, 0))
            goto cleanup;
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0))
        goto cleanup;

    for (DWORD i = 0; i < hash_len; i++) {
        sprintf(output + i * 2, "%02x", hash[i]);
    }
    output[40] = '\0';
    ret = 0;

cleanup:
    if (hHash) CryptDestroyHash(hHash);
    if (hProv) CryptReleaseContext(hProv, 0);
cleanup_file:
    fclose(f);
    return ret;
}
