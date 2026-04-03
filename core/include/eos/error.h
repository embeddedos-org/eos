// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_ERROR_H
#define EOS_ERROR_H

typedef enum {
    EOS_OK = 0,
    EOS_ERR_NOMEM,
    EOS_ERR_IO,
    EOS_ERR_PARSE,
    EOS_ERR_NOT_FOUND,
    EOS_ERR_INVALID,
    EOS_ERR_CYCLE,
    EOS_ERR_BUILD,
    EOS_ERR_FETCH,
    EOS_ERR_CHECKSUM,
    EOS_ERR_TOOLCHAIN,
    EOS_ERR_SYSTEM,
    EOS_ERR_COMMAND,
    EOS_ERR_OVERFLOW,
    EOS_ERR_COUNT
} EosResult;

static inline const char *eos_error_str(EosResult r) {
    switch (r) {
        case EOS_OK:            return "success";
        case EOS_ERR_NOMEM:     return "out of memory";
        case EOS_ERR_IO:        return "I/O error";
        case EOS_ERR_PARSE:     return "parse error";
        case EOS_ERR_NOT_FOUND: return "not found";
        case EOS_ERR_INVALID:   return "invalid argument";
        case EOS_ERR_CYCLE:     return "dependency cycle detected";
        case EOS_ERR_BUILD:     return "build failed";
        case EOS_ERR_FETCH:     return "fetch failed";
        case EOS_ERR_CHECKSUM:  return "checksum mismatch";
        case EOS_ERR_TOOLCHAIN: return "toolchain error";
        case EOS_ERR_SYSTEM:    return "system error";
        case EOS_ERR_COMMAND:   return "command failed";
        case EOS_ERR_OVERFLOW:  return "overflow";
        default:                  return "unknown error";
    }
}

#define EOS_CHECK(expr)                            \
    do {                                             \
        EosResult _r = (expr);                     \
        if (_r != EOS_OK) return _r;               \
    } while (0)

#define EOS_CHECK_NULL(ptr)                        \
    do {                                             \
        if (!(ptr)) return EOS_ERR_NOMEM;          \
    } while (0)

#endif /* EOS_ERROR_H */
