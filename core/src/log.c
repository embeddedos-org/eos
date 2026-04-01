// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eos/log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static EosLogLevel g_log_level = EOS_LOG_INFO;
static int g_color_enabled = 1;
static uint32_t g_outputs = EOS_LOG_OUTPUT_STDERR;
static FILE *g_log_file = NULL;
static void (*g_uart_write)(const char *buf, int len) = NULL;

/* Module-level filter: up to 16 modules */
#define EOS_LOG_MAX_MODULES 16
static struct { char name[32]; EosLogLevel level; } g_module_levels[EOS_LOG_MAX_MODULES];
static int g_module_count = 0;

/* Ring buffer for crash analysis */
static EosLogEntry g_ring[EOS_LOG_RING_SIZE];
static int g_ring_head = 0;
static int g_ring_count = 0;

void eos_log_set_level(EosLogLevel level) {
    if (level <= EOS_LOG_NONE) g_log_level = level;
}

EosLogLevel eos_log_get_level(void) {
    return g_log_level;
}

void eos_log_set_module_level(const char *module, EosLogLevel level) {
    if (!module) return;
    for (int i = 0; i < g_module_count; i++) {
        if (strcmp(g_module_levels[i].name, module) == 0) {
            g_module_levels[i].level = level;
            return;
        }
    }
    if (g_module_count < EOS_LOG_MAX_MODULES) {
        strncpy(g_module_levels[g_module_count].name, module, 31);
        g_module_levels[g_module_count].name[31] = '\0';
        g_module_levels[g_module_count].level = level;
        g_module_count++;
    }
}

void eos_log_set_output(uint32_t outputs) {
    g_outputs = outputs;
}

void eos_log_set_file(const char *path) {
    if (g_log_file && g_log_file != stderr && g_log_file != stdout) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    if (path) {
        g_log_file = fopen(path, "a");
        if (g_log_file) g_outputs |= EOS_LOG_OUTPUT_FILE;
    }
}

void eos_log_set_uart(void (*write_fn)(const char *buf, int len)) {
    g_uart_write = write_fn;
    if (write_fn) g_outputs |= EOS_LOG_OUTPUT_UART;
}

void eos_log_set_color(int enabled) {
    g_color_enabled = enabled;
}

static const char *level_str(EosLogLevel level) {
    switch (level) {
        case EOS_LOG_TRACE: return "TRACE";
        case EOS_LOG_DEBUG: return "DEBUG";
        case EOS_LOG_INFO:  return "INFO";
        case EOS_LOG_WARN:  return "WARN";
        case EOS_LOG_ERROR: return "ERROR";
        case EOS_LOG_FATAL: return "FATAL";
        default:            return "???";
    }
}

static const char *level_color(EosLogLevel level) {
    if (!g_color_enabled) return "";
    switch (level) {
        case EOS_LOG_TRACE: return "\033[90m";
        case EOS_LOG_DEBUG: return "\033[36m";
        case EOS_LOG_INFO:  return "\033[32m";
        case EOS_LOG_WARN:  return "\033[33m";
        case EOS_LOG_ERROR: return "\033[31m";
        case EOS_LOG_FATAL: return "\033[1;31m";
        default:            return "";
    }
}

static const char *color_reset(void) {
    return g_color_enabled ? "\033[0m" : "";
}

static uint32_t get_timestamp_ms(void) {
#ifdef _WIN32
    return (uint32_t)GetTickCount();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    return 0;
#endif
}

static void ring_push(EosLogLevel level, const char *module, const char *msg) {
    EosLogEntry *e = &g_ring[g_ring_head];
    e->level = level;
    e->timestamp_ms = get_timestamp_ms();
    e->module = module;
    strncpy(e->message, msg, sizeof(e->message) - 1);
    e->message[sizeof(e->message) - 1] = '\0';
    g_ring_head = (g_ring_head + 1) % EOS_LOG_RING_SIZE;
    if (g_ring_count < EOS_LOG_RING_SIZE) g_ring_count++;
}

int eos_log_ring_count(void) { return g_ring_count; }

const EosLogEntry *eos_log_ring_get(int index) {
    if (index < 0 || index >= g_ring_count) return NULL;
    int pos = (g_ring_head - g_ring_count + index + EOS_LOG_RING_SIZE) % EOS_LOG_RING_SIZE;
    return &g_ring[pos];
}

void eos_log_ring_dump(void) {
    fprintf(stderr, "=== Log Ring Buffer (%d entries) ===\n", g_ring_count);
    for (int i = 0; i < g_ring_count; i++) {
        const EosLogEntry *e = eos_log_ring_get(i);
        if (e) {
            fprintf(stderr, "[%u %5s] %s%s%s\n",
                    e->timestamp_ms, level_str(e->level),
                    e->module ? e->module : "", e->module ? ": " : "",
                    e->message);
        }
    }
}

void eos_log_ring_clear(void) {
    g_ring_head = 0;
    g_ring_count = 0;
}

static void emit(EosLogLevel level, const char *module, const char *formatted) {
    char line[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timebuf[20];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_info);

    int n;
    if (module) {
        n = snprintf(line, sizeof(line), "[%s %5s %s] %s\n",
                     timebuf, level_str(level), module, formatted);
    } else {
        n = snprintf(line, sizeof(line), "[%s %5s] %s\n",
                     timebuf, level_str(level), formatted);
    }
    if (n < 0) n = 0;
    if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;

    /* Ring buffer — always store */
    if (g_outputs & EOS_LOG_OUTPUT_RING)
        ring_push(level, module, formatted);

    /* stderr */
    if (g_outputs & EOS_LOG_OUTPUT_STDERR) {
#ifdef _WIN32
        if (g_color_enabled) {
            HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
            DWORD mode;
            if (GetConsoleMode(h, &mode))
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
#endif
        if (module)
            fprintf(stderr, "%s[%s %5s %s]%s %s\n",
                    level_color(level), timebuf, level_str(level), module,
                    color_reset(), formatted);
        else
            fprintf(stderr, "%s[%s %5s]%s %s\n",
                    level_color(level), timebuf, level_str(level),
                    color_reset(), formatted);
    }

    /* File */
    if ((g_outputs & EOS_LOG_OUTPUT_FILE) && g_log_file) {
        fputs(line, g_log_file);
        fflush(g_log_file);
    }

    /* UART */
    if ((g_outputs & EOS_LOG_OUTPUT_UART) && g_uart_write) {
        g_uart_write(line, n);
    }
}

void eos_log(EosLogLevel level, const char *fmt, ...) {
    if (level < g_log_level) return;

    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    emit(level, NULL, buf);
}

void eos_log_module(EosLogLevel level, const char *module, const char *fmt, ...) {
    /* Check module-specific level first */
    if (module) {
        for (int i = 0; i < g_module_count; i++) {
            if (strcmp(g_module_levels[i].name, module) == 0) {
                if (level < g_module_levels[i].level) return;
                goto emit_log;
            }
        }
    }
    /* Fall back to global level */
    if (level < g_log_level) return;

emit_log:;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    emit(level, module, buf);
}