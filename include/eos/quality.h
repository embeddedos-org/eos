// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 SQuaRE - Quality assurance macros

#ifndef EOS_QUALITY_H
#define EOS_QUALITY_H

#ifdef __GNUC__
#define EOS_WARN_UNUSED    __attribute__((warn_unused_result))
#define EOS_NONNULL(...)   __attribute__((nonnull(__VA_ARGS__)))
#define EOS_DEPRECATED(m)  __attribute__((deprecated(m)))
#define EOS_PURE           __attribute__((pure))
#define EOS_CONST          __attribute__((const))
#define EOS_PRINTF(f,a)    __attribute__((format(printf, f, a)))
#define EOS_PACKED         __attribute__((packed))
#define EOS_ALIGNED(n)     __attribute__((aligned(n)))
#define EOS_LIKELY(x)      __builtin_expect(!!(x), 1)
#define EOS_UNLIKELY(x)    __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
#define EOS_WARN_UNUSED    _Check_return_
#define EOS_NONNULL(...)
#define EOS_DEPRECATED(m)  __declspec(deprecated(m))
#define EOS_PURE
#define EOS_CONST
#define EOS_PRINTF(f,a)
#define EOS_PACKED
#define EOS_ALIGNED(n)     __declspec(align(n))
#define EOS_LIKELY(x)      (x)
#define EOS_UNLIKELY(x)    (x)
#else
#define EOS_WARN_UNUSED
#define EOS_NONNULL(...)
#define EOS_DEPRECATED(m)
#define EOS_PURE
#define EOS_CONST
#define EOS_PRINTF(f,a)
#define EOS_PACKED
#define EOS_ALIGNED(n)
#define EOS_LIKELY(x)      (x)
#define EOS_UNLIKELY(x)    (x)
#endif

#define EOS_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#define EOS_API_VERSION_MAJOR 0
#define EOS_API_VERSION_MINOR 1
#define EOS_API_VERSION_PATCH 0
#define EOS_API_VERSION       ((EOS_API_VERSION_MAJOR << 16) | \
                               (EOS_API_VERSION_MINOR << 8)  | \
                               EOS_API_VERSION_PATCH)

#define EOS_ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define EOS_MIN(a, b)        (((a) < (b)) ? (a) : (b))
#define EOS_MAX(a, b)        (((a) > (b)) ? (a) : (b))
#define EOS_CLAMP(x, lo, hi) (EOS_MIN(EOS_MAX((x), (lo)), (hi)))

#endif /* EOS_QUALITY_H */