#include "eos/log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

static EosLogLevel g_log_level = EOS_LOG_INFO;
static int g_color_enabled = 1;

void eos_log_set_level(EosLogLevel level) {
    g_log_level = level;
}

void eos_log_set_color(int enabled) {
    g_color_enabled = enabled;
}

static const char *level_str(EosLogLevel level) {
    switch (level) {
        case EOS_LOG_DEBUG: return "DEBUG";
        case EOS_LOG_INFO:  return "INFO";
        case EOS_LOG_WARN:  return "WARN";
        case EOS_LOG_ERROR: return "ERROR";
        default:              return "???";
    }
}

static const char *level_color(EosLogLevel level) {
    if (!g_color_enabled) return "";
    switch (level) {
        case EOS_LOG_DEBUG: return "\033[36m";   /* cyan */
        case EOS_LOG_INFO:  return "\033[32m";   /* green */
        case EOS_LOG_WARN:  return "\033[33m";   /* yellow */
        case EOS_LOG_ERROR: return "\033[31m";   /* red */
        default:              return "";
    }
}

static const char *color_reset(void) {
    return g_color_enabled ? "\033[0m" : "";
}

void eos_log(EosLogLevel level, const char *fmt, ...) {
    if (level < g_log_level) return;

#ifdef _WIN32
    if (g_color_enabled) {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD mode;
        if (GetConsoleMode(h, &mode)) {
            SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timebuf[20];
    strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm_info);

    fprintf(stderr, "%s[%s %5s]%s ",
            level_color(level), timebuf, level_str(level), color_reset());

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
}
