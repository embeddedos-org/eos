// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/package.h"
#include "eos/log.h"
#include "eos/cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#include <sys/types.h>
#define MKDIR(d) mkdir(d, 0755)
#endif

/*
 * Minimal SHA-256 implementation (FIPS 180-4).
 * No external dependencies — suitable for build-time checksum verification.
 */
static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (RR(x,2)^RR(x,13)^RR(x,22))
#define EP1(x) (RR(x,6)^RR(x,11)^RR(x,25))
#define SIG0(x) (RR(x,7)^RR(x,18)^((x)>>3))
#define SIG1(x) (RR(x,17)^RR(x,19)^((x)>>10))

typedef struct {
    uint32_t state[8];
    uint8_t  buf[64];
    uint64_t total;
} Sha256Ctx;

static void sha256_init(Sha256Ctx *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->total = 0;
}

static void sha256_transform(Sha256Ctx *ctx, const uint8_t *block) {
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2;
    for (int i=0;i<16;i++)
        w[i]=(uint32_t)block[i*4]<<24|(uint32_t)block[i*4+1]<<16|
             (uint32_t)block[i*4+2]<<8|(uint32_t)block[i*4+3];
    for (int i=16;i<64;i++)
        w[i]=SIG1(w[i-2])+w[i-7]+SIG0(w[i-15])+w[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for (int i=0;i<64;i++){
        t1=h+EP1(e)+CH(e,f,g)+sha256_k[i]+w[i];
        t2=EP0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

static void sha256_update(Sha256Ctx *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    size_t off = (size_t)(ctx->total % 64);
    ctx->total += len;
    while (len > 0) {
        size_t n = 64 - off;
        if (n > len) n = len;
        memcpy(ctx->buf + off, p, n);
        p += n; len -= n; off += n;
        if (off == 64) { sha256_transform(ctx, ctx->buf); off = 0; }
    }
}

static void sha256_final(Sha256Ctx *ctx, char *hex_out) {
    uint64_t bits = ctx->total * 8;
    size_t off = (size_t)(ctx->total % 64);
    ctx->buf[off++] = 0x80;
    if (off > 56) {
        memset(ctx->buf + off, 0, 64 - off);
        sha256_transform(ctx, ctx->buf);
        off = 0;
    }
    memset(ctx->buf + off, 0, 56 - off);
    for (int i=0;i<8;i++) ctx->buf[56+i]=(uint8_t)(bits>>(56-i*8));
    sha256_transform(ctx, ctx->buf);
    for (int i=0;i<8;i++)
        sprintf(hex_out+i*8, "%08x", ctx->state[i]);
    hex_out[64] = '\0';
}

static EosResult compute_file_sha256(const char *path, char *hex_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return EOS_ERR_IO;

    Sha256Ctx ctx;
    sha256_init(&ctx);

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        sha256_update(&ctx, buf, n);
    }
    fclose(fp);

    sha256_final(&ctx, hex_out);
    return EOS_OK;
}

static int is_git_url(const char *url) {
    return (strstr(url, ".git") != NULL ||
            strncmp(url, "git://", 6) == 0 ||
            strncmp(url, "git@", 4) == 0);
}

static int is_tarball(const char *url) {
    return (strstr(url, ".tar") != NULL ||
            strstr(url, ".tgz") != NULL ||
            strstr(url, ".zip") != NULL);
}

static EosResult fetch_git(const char *url, const char *dest) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "git clone --depth 1 \"%s\" \"%s\"", url, dest);
    EOS_INFO("Fetching (git): %s", url);
    int rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_FETCH;
}

static EosResult fetch_tarball(const char *url, const char *dest) {
    MKDIR(dest);

    char archive[EOS_MAX_PATH];
    const char *basename = strrchr(url, '/');
    if (!basename) basename = url;
    else basename++;
    snprintf(archive, sizeof(archive), "%s/%s", dest, basename);

    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "curl -fSL -o \"%s\" \"%s\"", archive, url);
#else
    snprintf(cmd, sizeof(cmd), "wget -q -O \"%s\" \"%s\"", archive, url);
#endif
    EOS_INFO("Fetching (download): %s", url);
    int rc = system(cmd);
    if (rc != 0) return EOS_ERR_FETCH;

    /* Extract */
    if (strstr(archive, ".tar.gz") || strstr(archive, ".tgz")) {
        snprintf(cmd, sizeof(cmd), "tar xzf \"%s\" -C \"%s\" --strip-components=1",
                 archive, dest);
    } else if (strstr(archive, ".tar.xz")) {
        snprintf(cmd, sizeof(cmd), "tar xJf \"%s\" -C \"%s\" --strip-components=1",
                 archive, dest);
    } else if (strstr(archive, ".tar.bz2")) {
        snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" --strip-components=1",
                 archive, dest);
    } else if (strstr(archive, ".zip")) {
        snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", archive, dest);
    } else {
        EOS_WARN("Unknown archive format: %s", archive);
        return EOS_ERR_INVALID;
    }

    EOS_INFO("Extracting: %s", archive);
    rc = system(cmd);
    return (rc == 0) ? EOS_OK : EOS_ERR_FETCH;
}

EosResult eos_fetch_source(const char *url, const char *dest_dir,
                               const char *expected_hash) {
    if (!url || url[0] == '\0') {
        EOS_WARN("No source URL specified, skipping fetch");
        return EOS_OK;
    }

    /* Determine archive path for checksum verification */
    char archive_path[EOS_MAX_PATH] = {0};
    if (is_tarball(url)) {
        const char *basename = strrchr(url, '/');
        if (!basename) basename = url;
        else basename++;
        snprintf(archive_path, sizeof(archive_path), "%s/%s", dest_dir, basename);
    }

    EosResult res;
    if (is_git_url(url)) {
        res = fetch_git(url, dest_dir);
    } else if (is_tarball(url)) {
        /* Download first (before extraction) for checksum */
        MKDIR(dest_dir);
        char cmd[2048];
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "curl -fSL -o \"%s\" \"%s\"", archive_path, url);
#else
        snprintf(cmd, sizeof(cmd), "wget -q -O \"%s\" \"%s\"", archive_path, url);
#endif
        EOS_INFO("Fetching (download): %s", url);
        int rc = system(cmd);
        if (rc != 0) return EOS_ERR_FETCH;

        /* Verify checksum BEFORE extraction */
        if (expected_hash && expected_hash[0]) {
            char computed[EOS_HASH_LEN];
            EosResult hash_res = compute_file_sha256(archive_path, computed);
            if (hash_res != EOS_OK) {
                EOS_ERROR("Cannot compute SHA256 for %s", archive_path);
                return EOS_ERR_CHECKSUM;
            }
            if (strcmp(computed, expected_hash) != 0) {
                EOS_ERROR("Checksum mismatch for %s", url);
                EOS_ERROR("  Expected: %s", expected_hash);
                EOS_ERROR("  Got:      %s", computed);
                return EOS_ERR_CHECKSUM;
            }
            EOS_INFO("Checksum OK: %s", computed);
        }

        /* Now extract */
        if (strstr(archive_path, ".tar.gz") || strstr(archive_path, ".tgz")) {
            snprintf(cmd, sizeof(cmd), "tar xzf \"%s\" -C \"%s\" --strip-components=1",
                     archive_path, dest_dir);
        } else if (strstr(archive_path, ".tar.xz")) {
            snprintf(cmd, sizeof(cmd), "tar xJf \"%s\" -C \"%s\" --strip-components=1",
                     archive_path, dest_dir);
        } else if (strstr(archive_path, ".tar.bz2")) {
            snprintf(cmd, sizeof(cmd), "tar xjf \"%s\" -C \"%s\" --strip-components=1",
                     archive_path, dest_dir);
        } else if (strstr(archive_path, ".zip")) {
            snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", archive_path, dest_dir);
        } else {
            return EOS_ERR_INVALID;
        }

        EOS_INFO("Extracting: %s", archive_path);
        rc = system(cmd);
        res = (rc == 0) ? EOS_OK : EOS_ERR_FETCH;
    } else {
        EOS_WARN("Assuming tarball for URL: %s", url);
        res = fetch_tarball(url, dest_dir);
    }

    return res;
}
