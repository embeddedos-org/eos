// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EOS_LOG_H
#define EOS_LOG_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EOS_LOG_TRACE = 0,
    EOS_LOG_DEBUG = 1,
    EOS_LOG_INFO  = 2,
    EOS_LOG_WARN  = 3,
    EOS_LOG_ERROR = 4,
    EOS_LOG_FATAL = 5,
    EOS_LOG_NONE  = 6
} EosLogLevel;

typedef enum {
    EOS_LOG_OUTPUT_STDERR = 0x01,
    EOS_LOG_OUTPUT_UART   = 0x02,
    EOS_LOG_OUTPUT_FILE   = 0x04,
    EOS_LOG_OUTPUT_RING   = 0x08,
    EOS_LOG_OUTPUT_SYSLOG = 0x10
} EosLogOutput;

typedef struct {
    EosLogLevel level;
    uint32_t    timestamp_ms;
    const char *module;
    char        message[128];
} EosLogEntry;

#define EOS_LOG_RING_SIZE 64

/* Runtime log level control — no recompile needed */
void eos_log_set_level(EosLogLevel level);
EosLogLevel eos_log_get_level(void);

/* Module-level filtering: eos_log_set_module_level("hal", EOS_LOG_DEBUG) */
void eos_log_set_module_level(const char *module, EosLogLevel level);

/* Output target control — bitwise OR of EosLogOutput */
void eos_log_set_output(uint32_t outputs);
void eos_log_set_file(const char *path);
void eos_log_set_uart(void (*write_fn)(const char *buf, int len));

/* Color control */
void eos_log_set_color(int enabled);

/* Core log function */
void eos_log(EosLogLevel level, const char *fmt, ...);
void eos_log_module(EosLogLevel level, const char *module, const char *fmt, ...);

/* Ring buffer — crash-safe log history */
int  eos_log_ring_count(void);
const EosLogEntry *eos_log_ring_get(int index);
void eos_log_ring_dump(void);
void eos_log_ring_clear(void);

/* Convenience macros */
#define EOS_TRACE(...) eos_log(EOS_LOG_TRACE, __VA_ARGS__)
#define EOS_DEBUG(...) eos_log(EOS_LOG_DEBUG, __VA_ARGS__)
#define EOS_INFO(...)  eos_log(EOS_LOG_INFO,  __VA_ARGS__)
#define EOS_WARN(...)  eos_log(EOS_LOG_WARN,  __VA_ARGS__)
#define EOS_ERROR(...) eos_log(EOS_LOG_ERROR, __VA_ARGS__)
#define EOS_FATAL(...) eos_log(EOS_LOG_FATAL, __VA_ARGS__)

/* Module-scoped macros — define EOS_LOG_MODULE before including */
#ifdef EOS_LOG_MODULE
#define MLOG_TRACE(...) eos_log_module(EOS_LOG_TRACE, EOS_LOG_MODULE, __VA_ARGS__)
#define MLOG_DEBUG(...) eos_log_module(EOS_LOG_DEBUG, EOS_LOG_MODULE, __VA_ARGS__)
#define MLOG_INFO(...)  eos_log_module(EOS_LOG_INFO,  EOS_LOG_MODULE, __VA_ARGS__)
#define MLOG_WARN(...)  eos_log_module(EOS_LOG_WARN,  EOS_LOG_MODULE, __VA_ARGS__)
#define MLOG_ERROR(...) eos_log_module(EOS_LOG_ERROR, EOS_LOG_MODULE, __VA_ARGS__)
#define MLOG_FATAL(...) eos_log_module(EOS_LOG_FATAL, EOS_LOG_MODULE, __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* EOS_LOG_H */