// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file main.c
 * @brief EoS Example: UART Echo — Basic serial communication
 *
 * Initializes UART, prints a welcome message, then echoes back
 * every byte received. Demonstrates UART read/write operations.
 */

#include <eos/hal.h>
#include <stdio.h>
#include <string.h>

#define UART_PORT 0
#define BAUDRATE  115200

static void uart_print(uint8_t port, const char *str)
{
    eos_uart_write(port, (const uint8_t *)str, strlen(str));
}

int main(void)
{
    eos_hal_init();

    eos_uart_config_t uart_cfg = {
        .port      = UART_PORT,
        .baudrate  = BAUDRATE,
        .data_bits = 8,
        .parity    = EOS_UART_PARITY_NONE,
        .stop_bits = EOS_UART_STOP_1,
    };
    eos_uart_init(&uart_cfg);

    uart_print(UART_PORT, "\r\n=== EoS UART Echo ===\r\n");
    uart_print(UART_PORT, "Type characters and they will be echoed back.\r\n");
    uart_print(UART_PORT, "Running at 115200 baud, 8N1\r\n\r\n");

    printf("[uart-echo] UART%d initialized at %d baud\n", UART_PORT, BAUDRATE);
    printf("[uart-echo] Waiting for input...\n");

    uint8_t rx_buf[64];

    while (1) {
        int n = eos_uart_read(UART_PORT, rx_buf, sizeof(rx_buf), 100);
        if (n > 0) {
            eos_uart_write(UART_PORT, rx_buf, (size_t)n);
            printf("[uart-echo] Echoed %d byte(s)\n", n);
        }
    }

    return 0;
}
