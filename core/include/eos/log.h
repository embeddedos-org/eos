#ifndef EOS_LOG_H
#define EOS_LOG_H

#include <stdio.h>

typedef enum {
    EOS_LOG_DEBUG,
    EOS_LOG_INFO,
    EOS_LOG_WARN,
    EOS_LOG_ERROR
} EosLogLevel;

void eos_log_set_level(EosLogLevel level);
void eos_log_set_color(int enabled);
void eos_log(EosLogLevel level, const char *fmt, ...);

#define EOS_DEBUG(...) eos_log(EOS_LOG_DEBUG, __VA_ARGS__)
#define EOS_INFO(...)  eos_log(EOS_LOG_INFO,  __VA_ARGS__)
#define EOS_WARN(...)  eos_log(EOS_LOG_WARN,  __VA_ARGS__)
#define EOS_ERROR(...) eos_log(EOS_LOG_ERROR, __VA_ARGS__)

#endif /* EOS_LOG_H */
