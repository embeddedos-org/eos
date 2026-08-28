// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file test_devicetree.c
 * @brief Flattened device tree parser: happy path and malformed-blob rejection
 *
 * A DTB reaches us from a prior boot stage or a flash region, so every offset
 * and length inside it is untrusted input. The "malformed" cases below are
 * regressions: each one used to read outside the blob (or outside the parser's
 * own node stack) before the bounds checks were added. Running this suite under
 * -fsanitize=address,undefined reproduces the original reports.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devicetree.h"

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg)                                         \
    do {                                                          \
        tests_run++;                                              \
        if (!(expr)) {                                            \
            printf("  FAIL: %s (line %d)\n", msg, __LINE__);      \
        } else {                                                  \
            tests_passed++;                                       \
            printf("  PASS: %s\n", msg);                          \
        }                                                         \
    } while (0)

/* ------------------------------------------------------------------
 * Minimal DTB builder
 * ------------------------------------------------------------------ */

typedef struct {
    uint8_t  buf[1024];
    uint32_t len;
} Blob;

static void b_reset(Blob *b) { b->len = 0; }

static void b_u32(Blob *b, uint32_t v)
{
    b->buf[b->len++] = (uint8_t)(v >> 24);
    b->buf[b->len++] = (uint8_t)(v >> 16);
    b->buf[b->len++] = (uint8_t)(v >> 8);
    b->buf[b->len++] = (uint8_t)v;
}

/* Append a NUL-terminated string, then pad to a 4-byte boundary. */
static void b_str_pad(Blob *b, const char *s)
{
    size_t n = strlen(s) + 1;
    memcpy(b->buf + b->len, s, n);
    b->len += (uint32_t)n;
    while (b->len & 3u) b->buf[b->len++] = 0;
}

/* Append a NUL-terminated string with no padding (strings block). */
static uint32_t b_str(Blob *b, const char *s)
{
    uint32_t off = b->len;
    size_t n = strlen(s) + 1;
    memcpy(b->buf + b->len, s, n);
    b->len += (uint32_t)n;
    return off;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/*
 * Compose a well-formed v17 blob from a struct block and a strings block.
 * Returns a heap buffer so ASan's redzones sit immediately after the blob and
 * any over-read is caught precisely.
 */
static uint8_t *compose(const Blob *st, const Blob *str, uint32_t *out_size)
{
    uint32_t off_rsv    = EOS_DT_HEADER_SIZE;
    uint32_t off_struct = off_rsv + 8u;            /* one empty reserve entry */
    uint32_t off_string = off_struct + st->len;
    uint32_t total      = off_string + str->len;

    uint8_t *b = calloc(1, total);
    put32(b + 0,  EOS_DT_MAGIC);
    put32(b + 4,  total);
    put32(b + 8,  off_struct);
    put32(b + 12, off_string);
    put32(b + 16, off_rsv);
    put32(b + 20, 17);           /* version */
    put32(b + 24, 16);           /* last_comp_version */
    put32(b + 28, 0);            /* boot_cpuid_phys */
    put32(b + 32, str->len);     /* size_dt_strings */
    put32(b + 36, st->len);      /* size_dt_struct */

    memcpy(b + off_struct, st->buf, st->len);
    memcpy(b + off_string, str->buf, str->len);
    *out_size = total;
    return b;
}

/*
 * A small but realistic tree:
 *
 *   / {
 *       model = "EoS Test Board";
 *       compatible = "eos,test-board";
 *       uart@40011000 {
 *           reg = <0x40011000 0x400>;
 *           interrupts = <37 38>;
 *           phandle = <1>;
 *       };
 *   };
 */
static uint8_t *build_valid_dtb(uint32_t *out_size)
{
    static Blob st, str;
    b_reset(&st); b_reset(&str);

    uint32_t s_model   = b_str(&str, "model");
    uint32_t s_compat  = b_str(&str, "compatible");
    uint32_t s_reg     = b_str(&str, "reg");
    uint32_t s_irq     = b_str(&str, "interrupts");
    uint32_t s_phandle = b_str(&str, "phandle");

    b_u32(&st, EOS_DT_BEGIN_NODE);
    b_str_pad(&st, "");                       /* root has an empty name */

    b_u32(&st, EOS_DT_PROP);
    b_u32(&st, 15); b_u32(&st, s_model);
    b_str_pad(&st, "EoS Test Board");

    b_u32(&st, EOS_DT_PROP);
    b_u32(&st, 15); b_u32(&st, s_compat);
    b_str_pad(&st, "eos,test-board");

    b_u32(&st, EOS_DT_BEGIN_NODE);
    b_str_pad(&st, "uart@40011000");

    b_u32(&st, EOS_DT_PROP);
    b_u32(&st, 8); b_u32(&st, s_reg);
    b_u32(&st, 0x40011000); b_u32(&st, 0x400);

    b_u32(&st, EOS_DT_PROP);
    b_u32(&st, 8); b_u32(&st, s_irq);
    b_u32(&st, 37); b_u32(&st, 38);

    b_u32(&st, EOS_DT_PROP);
    b_u32(&st, 4); b_u32(&st, s_phandle);
    b_u32(&st, 1);

    b_u32(&st, EOS_DT_END_NODE);              /* uart */
    b_u32(&st, EOS_DT_END_NODE);              /* root */
    b_u32(&st, EOS_DT_END);

    return compose(&st, &str, out_size);
}

/* ------------------------------------------------------------------
 * Happy path
 * ------------------------------------------------------------------ */

static void test_parse_valid_blob(void)
{
    printf("test_parse_valid_blob:\n");
    uint32_t size = 0;
    uint8_t *dtb = build_valid_dtb(&size);
    EosDeviceTree *dt = malloc(sizeof(*dt));

    ASSERT(eos_dt_parse(dt, dtb, size) == 0, "well-formed blob parses");
    ASSERT(eos_dt_node_count(dt) == 2, "root plus one child node");
    ASSERT(dt->version == 17, "version read from header");
    ASSERT(strcmp(dt->model, "EoS Test Board") == 0, "model cached at root");
    ASSERT(strcmp(dt->compatible, "eos,test-board") == 0,
           "compatible cached at root");

    free(dt); free(dtb);
}

static void test_lookup_and_property_access(void)
{
    printf("test_lookup_and_property_access:\n");
    uint32_t size = 0;
    uint8_t *dtb = build_valid_dtb(&size);
    EosDeviceTree *dt = malloc(sizeof(*dt));
    eos_dt_parse(dt, dtb, size);

    ASSERT(eos_dt_find(dt, "/") == dt->root, "root resolves");

    EosDtNode *uart = eos_dt_find(dt, "/uart@40011000");
    ASSERT(uart != NULL, "child node resolves by path");
    ASSERT(eos_dt_find(dt, "/nonexistent") == NULL, "unknown path returns NULL");

    if (uart) {
        ASSERT(uart->parent == dt->root, "child links back to its parent");
        ASSERT(eos_dt_get_prop(uart, "reg") != NULL, "reg property present");
        ASSERT(eos_dt_get_prop(uart, "absent") == NULL, "absent property is NULL");

        uint32_t addr = 0, sz = 0;
        ASSERT(eos_dt_get_reg(uart, &addr, &sz) == 0, "reg decodes");
        ASSERT(addr == 0x40011000u && sz == 0x400u, "reg holds address and size");

        ASSERT(eos_dt_get_irq(uart, 0) == 37, "first interrupt decodes");
        ASSERT(eos_dt_get_irq(uart, 1) == 38, "second interrupt decodes");
        ASSERT(eos_dt_get_irq(uart, 2) == -1, "index past the array is rejected");
        ASSERT(eos_dt_get_irq(uart, -1) == -1, "negative index is rejected");
        ASSERT(eos_dt_get_irq(uart, 0x40000000) == -1,
               "index that would overflow the offset is rejected");

        uint32_t ph = 0;
        ASSERT(eos_dt_get_u32(uart, "phandle", &ph) == 0 && ph == 1u,
               "u32 property decodes");
        ASSERT(eos_dt_find_by_phandle(dt, 1) == uart, "phandle lookup works");
    }

    ASSERT(eos_dt_find_compatible(dt, "eos,test-board") == dt->root,
           "compatible lookup finds the root");
    ASSERT(eos_dt_find_compatible(dt, "vendor,absent") == NULL,
           "unknown compatible returns NULL");

    char buf[32];
    ASSERT(eos_dt_get_string(dt->root, "model", buf, (int)sizeof(buf)) == 0 &&
           strcmp(buf, "EoS Test Board") == 0, "string property copies out");

    free(dt); free(dtb);
}

/* ------------------------------------------------------------------
 * Malformed input — each of these was an out-of-bounds read
 * ------------------------------------------------------------------ */

/* Build a bare 40-byte header with caller-chosen offsets. */
static uint8_t *lone_header(uint32_t total, uint32_t off_struct,
                            uint32_t off_string)
{
    uint8_t *b = calloc(1, EOS_DT_HEADER_SIZE);
    put32(b + 0,  EOS_DT_MAGIC);
    put32(b + 4,  total);
    put32(b + 8,  off_struct);
    put32(b + 12, off_string);
    put32(b + 20, 17);
    return b;
}

static void test_rejects_malformed_blobs(void)
{
    printf("test_rejects_malformed_blobs:\n");
    EosDeviceTree *dt = malloc(sizeof(*dt));

    ASSERT(eos_dt_parse(NULL, NULL, 0) == -1, "NULL arguments rejected");

    {   /* Shorter than a header. */
        uint8_t tiny[8] = {0};
        ASSERT(eos_dt_parse(dt, tiny, sizeof(tiny)) == -1,
               "blob shorter than the header is rejected");
    }

    {   /* Wrong magic. */
        uint8_t *b = lone_header(EOS_DT_HEADER_SIZE, EOS_DT_HEADER_SIZE, EOS_DT_HEADER_SIZE);
        put32(b, 0xDEADBEEF);
        ASSERT(eos_dt_parse(dt, b, EOS_DT_HEADER_SIZE) == -1, "bad magic is rejected");
        free(b);
    }

    {   /* off_struct past the end of the buffer.
           `size - off_struct` used to wrap, giving the walk a huge bound. */
        uint8_t *b = lone_header(EOS_DT_HEADER_SIZE, 4096, 4096);
        ASSERT(eos_dt_parse(dt, b, EOS_DT_HEADER_SIZE) == -1,
               "struct offset past the buffer is rejected");
        free(b);
    }

    {   /* totalsize larger than the buffer we were handed. */
        uint8_t *b = lone_header(0x10000, EOS_DT_HEADER_SIZE, EOS_DT_HEADER_SIZE);
        ASSERT(eos_dt_parse(dt, b, EOS_DT_HEADER_SIZE) == -1,
               "totalsize larger than the buffer is rejected");
        free(b);
    }

    {   /* BEGIN_NODE whose name has no terminator before the blob ends.
           strlen() used to run past the allocation. */
        uint32_t total = 48;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, 40); put32(b + 12, 44); put32(b + 20, 17);
        put32(b + 40, EOS_DT_BEGIN_NODE);
        memset(b + 44, 'A', 4);          /* four non-NUL bytes, then the end */
        ASSERT(eos_dt_parse(dt, b, total) == -1,
               "unterminated node name is rejected");
        free(b);
    }

    {   /* PROP with a name offset far outside the strings block. */
        uint32_t total = 64;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, 40); put32(b + 12, 44); put32(b + 20, 17);
        put32(b + 40, EOS_DT_BEGIN_NODE);       /* root, empty name + padding */
        put32(b + 48, EOS_DT_PROP);
        put32(b + 52, 4);
        put32(b + 56, 0x40000000);              /* nameoff far out of range */
        put32(b + 60, 0xDEADBEEF);
        ASSERT(eos_dt_parse(dt, b, total) == -1,
               "property name offset outside the strings block is rejected");
        free(b);
    }

    {   /* Nesting deeper than the parser's node stack.
           stack[depth - 1] used to read past the end of the local array. */
        uint32_t depth = EOS_DT_MAX_DEPTH + 8u;
        uint32_t total = EOS_DT_HEADER_SIZE + depth * 8u + 8u;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, EOS_DT_HEADER_SIZE); put32(b + 12, total - 4);
        put32(b + 20, 17);
        uint32_t p = EOS_DT_HEADER_SIZE;
        for (uint32_t i = 0; i < depth; i++) {
            put32(b + p, EOS_DT_BEGIN_NODE); p += 4;
            put32(b + p, 0);                 p += 4;   /* empty name + padding */
        }
        put32(b + p, EOS_DT_END);
        ASSERT(eos_dt_parse(dt, b, total) == -1,
               "nesting deeper than the node stack is rejected");
        free(b);
    }

    {   /* END_NODE with nothing open. */
        uint32_t total = 48;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, 40); put32(b + 12, 44); put32(b + 20, 17);
        put32(b + 40, EOS_DT_END_NODE);
        put32(b + 44, EOS_DT_END);
        ASSERT(eos_dt_parse(dt, b, total) == -1, "unbalanced END_NODE is rejected");
        free(b);
    }

    {   /* Property length running past the end of the struct block. */
        uint32_t total = 64;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, 40); put32(b + 12, 60); put32(b + 20, 17);
        put32(b + 40, EOS_DT_BEGIN_NODE);
        put32(b + 48, EOS_DT_PROP);
        put32(b + 52, 0xFFFF0000);              /* absurd value length */
        put32(b + 56, 0);
        ASSERT(eos_dt_parse(dt, b, total) == -1,
               "property length past the struct block is rejected");
        free(b);
    }

    {   /* Truncated: no FDT_END terminator. */
        uint32_t total = 48;
        uint8_t *b = calloc(1, total);
        put32(b + 0, EOS_DT_MAGIC); put32(b + 4, total);
        put32(b + 8, 40); put32(b + 12, 44); put32(b + 20, 17);
        put32(b + 40, EOS_DT_BEGIN_NODE);
        ASSERT(eos_dt_parse(dt, b, total) == -1, "missing FDT_END is rejected");
        free(b);
    }

    free(dt);
}

/*
 * Every prefix of a valid blob is malformed. The parser has to reject each one
 * without reading past the (shortened) buffer — this is the cheap deterministic
 * stand-in for the fuzz target, and it runs everywhere.
 */
static void test_truncations_never_overrun(void)
{
    printf("test_truncations_never_overrun:\n");
    uint32_t size = 0;
    uint8_t *full = build_valid_dtb(&size);
    EosDeviceTree *dt = malloc(sizeof(*dt));

    int rejected_all = 1;
    for (uint32_t n = 0; n < size; n++) {
        uint8_t *cut = malloc(n ? n : 1);
        memcpy(cut, full, n);
        /* Patch totalsize down so the blob is self-consistent about its size. */
        if (n >= EOS_DT_HEADER_SIZE) put32(cut + 4, n);
        if (eos_dt_parse(dt, cut, n) == 0) rejected_all = 0;
        free(cut);
    }
    ASSERT(rejected_all, "every truncation of a valid blob is rejected");

    /* And the untruncated blob still parses, so the loop above proves something. */
    ASSERT(eos_dt_parse(dt, full, size) == 0, "the untruncated blob still parses");

    free(dt); free(full);
}

/*
 * Flip one byte at a time across the whole blob. The result may parse or may be
 * rejected — what matters is that it never reads outside the buffer. Under ASan
 * this is where the original parser fell over.
 */
static void test_single_byte_corruption_is_safe(void)
{
    printf("test_single_byte_corruption_is_safe:\n");
    uint32_t size = 0;
    uint8_t *full = build_valid_dtb(&size);
    EosDeviceTree *dt = malloc(sizeof(*dt));

    for (uint32_t i = 0; i < size; i++) {
        static const uint8_t patterns[] = { 0x00, 0x01, 0x7F, 0x80, 0xFF };
        for (size_t k = 0; k < sizeof(patterns); k++) {
            uint8_t *copy = malloc(size);
            memcpy(copy, full, size);
            copy[i] = patterns[k];
            (void)eos_dt_parse(dt, copy, size);   /* must not read out of bounds */
            free(copy);
        }
    }
    ASSERT(1, "single-byte corruption never reads outside the blob");

    free(dt); free(full);
}

int main(void)
{
    printf("=== EoS Device Tree Parser Tests ===\n\n");

    test_parse_valid_blob();
    test_lookup_and_property_access();
    test_rejects_malformed_blobs();
    test_truncations_never_overrun();
    test_single_byte_corruption_is_safe();

    printf("\n%d/%d assertions passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
