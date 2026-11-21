// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

#include "eos/log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Everything eos_log() hands the UART writer, back to back. */
static char g_uart[2048];
static size_t g_uart_len;

static void capture_uart(const char *buf, int len)
{
    assert(len >= 0);
    assert(g_uart_len + (size_t)len < sizeof(g_uart));
    memcpy(g_uart + g_uart_len, buf, (size_t)len);
    g_uart_len += (size_t)len;
    g_uart[g_uart_len] = '\0';
}

static int count_char(const char *s, char c)
{
    int n = 0;
    for (; *s; s++) {
        if (*s == c) n++;
    }
    return n;
}

int main(void)
{
    char long_msg[241];
    memset(long_msg, 'x', sizeof(long_msg) - 1);
    long_msg[sizeof(long_msg) - 1] = '\0';

    eos_log_set_color(0);
    eos_log_set_level(EOS_LOG_INFO);
    eos_log_set_uart(capture_uart);
    eos_log_set_output(EOS_LOG_OUTPUT_UART);

    /* A short record ends in a newline. */
    eos_log(EOS_LOG_INFO, "short");
    assert(g_uart_len > 0);
    assert(g_uart[g_uart_len - 1] == '\n');
    assert(count_char(g_uart, '\n') == 1);

    /* A record too long for the line buffer still ends in a newline, so
     * the record after it starts on its own line. */
    g_uart_len = 0;
    g_uart[0] = '\0';
    eos_log(EOS_LOG_INFO, "%s", long_msg);
    assert(g_uart_len > 0);
    assert(g_uart[g_uart_len - 1] == '\n');
    eos_log_module(EOS_LOG_WARN, "net", "%s", long_msg);
    assert(g_uart[g_uart_len - 1] == '\n');
    eos_log(EOS_LOG_INFO, "after");
    assert(count_char(g_uart, '\n') == 3);
    assert(strstr(g_uart, "\n[") != NULL);
    {
        const char *last = strrchr(g_uart, '[');
        assert(last != NULL && last > g_uart && last[-1] == '\n');
        assert(strstr(last, "after\n") != NULL);
    }

    /* The file output gets the same lines. */
    const char *path = "test_log_tmp.log";
    remove(path);
    eos_log_set_file(path);
    eos_log_set_output(EOS_LOG_OUTPUT_FILE);
    eos_log(EOS_LOG_INFO, "%s", long_msg);
    eos_log(EOS_LOG_INFO, "after");
    eos_log_set_file(NULL);

    FILE *fp = fopen(path, "r");
    assert(fp != NULL);
    char line[512];
    int lines = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        assert(strchr(line, '\n') != NULL);
        lines++;
    }
    fclose(fp);
    remove(path);
    assert(lines == 2);

    printf("test_log: all checks passed\n");
    return 0;
}
