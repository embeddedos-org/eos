# Comprehensive Debugging Guide

Debug EoS firmware on ARM Cortex-M targets using GDB, OpenOCD, JTAG/SWD, and
built-in EoS diagnostic tools.

---

## GDB + OpenOCD Setup

### Start OpenOCD

```bash
# STM32F407 Discovery (ST-Link/V2)
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg

# STM32H743 Nucleo (ST-Link/V2-1)
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

# Generic CMSIS-DAP probe
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg

# J-Link
openocd -f interface/jlink.cfg -f target/stm32f4x.cfg -c "transport select swd"
```

Expected output:

```
Info : Listening on port 3333 for gdb connections
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : STLINK V2J37S7 (API v2) VID:PID 0483:3748
Info : Target voltage: 2.886034
Info : stm32f4x.cpu: Cortex-M4 r0p1 processor detected
Info : stm32f4x.cpu: target state: halted
```

### Connect GDB

```bash
arm-none-eabi-gdb build-stm32/eos-firmware.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

### Recommended `.gdbinit` Template

Place in your project root as `.gdbinit`:

```gdb
# ── Connection ──────────────────────────────────────────
target remote :3333
monitor reset halt

# ── Display settings ────────────────────────────────────
set print pretty on
set print elements 256
set pagination off
set confirm off
set mem inaccessible-by-default off

# ── Breakpoints ─────────────────────────────────────────
break main
break HardFault_Handler
break BusFault_Handler
break MemManage_Handler
break UsageFault_Handler

# ── Cortex-M fault register macro ──────────────────────
define fault_regs
  echo \n=== Fault Status Registers ===\n
  echo CFSR  (Configurable Fault Status):  \n
  x/1xw 0xE000ED28
  echo HFSR  (Hard Fault Status):          \n
  x/1xw 0xE000ED2C
  echo DFSR  (Debug Fault Status):         \n
  x/1xw 0xE000ED30
  echo MMFAR (MemManage Fault Address):    \n
  x/1xw 0xE000ED34
  echo BFAR  (Bus Fault Address):          \n
  x/1xw 0xE000ED38
  echo AFSR  (Auxiliary Fault Status):     \n
  x/1xw 0xE000ED3C
end

# ── Stack info macro ───────────────────────────────────
define stack_info
  echo \n=== Stack Info ===\n
  echo MSP: \n
  monitor reg msp
  echo PSP: \n
  monitor reg psp
  echo CONTROL: \n
  monitor reg control
end

# ── RCC clock status macro (STM32F4) ──────────────────
define rcc_status
  echo \n=== RCC Clock Status ===\n
  echo AHB1ENR: \n
  x/1xw 0x40023830
  echo APB1ENR: \n
  x/1xw 0x40023840
  echo APB2ENR: \n
  x/1xw 0x40023844
end

# ── Reflash shortcut ───────────────────────────────────
define reflash
  monitor reset halt
  load
  monitor reset run
end

# ── Start ──────────────────────────────────────────────
load
continue
```

Enable local `.gdbinit` in `~/.gdbinit`:

```gdb
set auto-load local-gdbinit on
add-auto-load-safe-path /path/to/EoS/eos
```

---

## JTAG vs SWD

### When to Use Which

| Feature | SWD | JTAG |
|---------|-----|------|
| **Pin count** | 2 (SWDIO + SWCLK) | 4+ (TMS, TCK, TDI, TDO) |
| **Speed** | Up to 4 MHz typical | Up to 10+ MHz |
| **Daisy-chain** | ❌ Single target | ✅ Multiple targets |
| **Trace (SWO)** | ✅ Via SWO pin | ✅ Via TDO or trace port |
| **Best for** | Single-MCU dev | Multi-core, production test |
| **ST-Link** | ✅ Default | ❌ SWD only on most probes |

**Rule of thumb:** Use **SWD** for daily development. Use **JTAG** for
daisy-chaining or maximum trace speed.

### SWD Pin Connections

| Signal | STM32F407 Pin | 10-pin SWD Header |
|--------|---------------|-------------------|
| SWDIO | PA13 | Pin 2 |
| SWCLK | PA14 | Pin 4 |
| SWO | PB3 | Pin 6 (optional) |
| NRST | NRST | Pin 10 |
| GND | GND | Pin 3, 5, 9 |
| VCC | VDD | Pin 1 |

### JTAG Pin Connections

| Signal | STM32F407 Pin | 20-pin JTAG Header |
|--------|---------------|---------------------|
| TMS | PA13 | Pin 7 |
| TCK | PA14 | Pin 9 |
| TDI | PA15 | Pin 5 |
| TDO | PB3 | Pin 13 |
| TRST | PB4 | Pin 3 (optional) |
| NRST | NRST | Pin 15 |
| GND | GND | Pin 4, 6, 8, 10, 12, 14, 16, 18, 20 |

### Configuring SWD Speed

```bash
openocd -f interface/stlink-v2.cfg \
  -c "adapter speed 4000" \
  -f target/stm32f4x.cfg
```

If you encounter `WAIT/FAULT` errors, reduce the speed:

```bash
openocd -f interface/stlink-v2.cfg \
  -c "adapter speed 1000" \
  -f target/stm32f4x.cfg
```

---

## Hard Fault Analysis

When a hard fault occurs, the Cortex-M core pushes exception context to the
stack and vectors to `HardFault_Handler`.

### Key Fault Registers

| Register | Address | Purpose |
|----------|---------|---------|
| **CFSR** | `0xE000ED28` | Configurable Fault Status (MMFSR + BFSR + UFSR) |
| **HFSR** | `0xE000ED2C` | Hard Fault Status |
| **MMFAR** | `0xE000ED34` | MemManage Fault Address (valid if CFSR.MMARVALID=1) |
| **BFAR** | `0xE000ED38` | Bus Fault Address (valid if CFSR.BFARVALID=1) |

### Reading Fault Registers in GDB

```gdb
(gdb) x/1xw 0xE000ED28
0xe000ed28:  0x00000400

(gdb) x/1xw 0xE000ED2C
0xe000ed2c:  0x40000000

(gdb) x/1xw 0xE000ED38
0xe000ed38:  0x20030000
```

### Decoding CFSR (0xE000ED28)

```
Bits [31:16] — UFSR (Usage Fault Status Register)
Bits [15:8]  — BFSR (Bus Fault Status Register)
Bits [7:0]   — MMFSR (MemManage Fault Status Register)
```

**MMFSR (bits 7:0):**

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | IACCVIOL | Instruction access violation |
| 1 | DACCVIOL | Data access violation |
| 3 | MUNSTKERR | MemManage fault on unstacking |
| 4 | MSTKERR | MemManage fault on stacking |
| 5 | MLSPERR | MemManage fault during FP lazy preserve |
| 7 | MMARVALID | MMFAR holds a valid address |

**BFSR (bits 15:8):**

| Bit | Name | Meaning |
|-----|------|---------|
| 8 | IBUSERR | Instruction bus error |
| 9 | PRECISERR | Precise data bus error (BFAR valid) |
| 10 | IMPRECISERR | Imprecise data bus error (BFAR invalid) |
| 11 | UNSTKERR | Bus fault on unstacking |
| 12 | STKERR | Bus fault on stacking |
| 13 | LSPERR | Bus fault during FP lazy preserve |
| 15 | BFARVALID | BFAR holds a valid address |

**UFSR (bits 31:16):**

| Bit | Name | Meaning |
|-----|------|---------|
| 16 | UNDEFINSTR | Undefined instruction |
| 17 | INVSTATE | Invalid state (Thumb bit cleared) |
| 18 | INVPC | Invalid PC load |
| 19 | NOCP | No coprocessor |
| 24 | UNALIGNED | Unaligned access |
| 25 | DIVBYZERO | Divide by zero (if enabled in CCR) |

### Decoding HFSR (0xE000ED2C)

| Bit | Name | Meaning |
|-----|------|---------|
| 1 | VECTTBL | Vector table read error |
| 30 | FORCED | Escalated from configurable fault |
| 31 | DEBUGEVT | Hard fault from debug event |

### Recovering the Faulting PC

The Cortex-M pushes 8 registers to the active stack on fault:

```
Stack (low → high):
  [SP+0x00] R0      [SP+0x04] R1
  [SP+0x08] R2      [SP+0x0C] R3
  [SP+0x10] R12     [SP+0x14] LR  ← caller return address
  [SP+0x18] PC      ← faulting instruction
  [SP+0x1C] xPSR
```

In GDB, once stopped in `HardFault_Handler`:

```gdb
# Check which stack was active (bit 2 of EXC_RETURN in LR)
(gdb) info registers lr
lr  0xfffffffd   # bit 2 set → PSP was in use

# Read stacked PC from PSP
(gdb) set $sp_val = $psp
(gdb) x/1xw ($sp_val + 0x18)
0x20001f98:  0x08003a42

# Map to source
(gdb) info line *0x08003a42
Line 127 of "src/sensor.c" starts at address 0x08003a40
```

### Example: Diagnosing a Bus Fault

```
[FAULT] HardFault triggered!
[FAULT] CFSR  = 0x00000400  → BFSR.IMPRECISERR
[FAULT] HFSR  = 0x40000000  → FORCED
```

IMPRECISERR means the write was buffered. To make it precise for debugging:

```c
// Disable write buffering (slower but BFAR becomes valid)
*(volatile uint32_t *)0xE000E008 |= (1 << 1);  // ACTLR.DISDEFWBUF
```

---

## Stack Overflow Detection

### 1. MPU Guard Pages

EoS configures the MPU to place a non-accessible guard region at the bottom of
each task stack:

```c
#define EOS_MAIN_STACK_SIZE   4096
#define EOS_TASK_STACK_SIZE   2048
#define EOS_STACK_GUARD_SIZE  32    // 32-byte MPU guard region
```

When the guard triggers:

```
[FAULT] MemManage fault — stack overflow detected
[FAULT] MMFAR = 0x2000FFF0 (guard region of task "sensor_read")
[FAULT] Task stack: base=0x20010000, size=2048, guard=0x2000F800..0x2000F820
```

### 2. Stack Painting (Watermark Analysis)

Fill unused stack with a known pattern at boot, then measure peak usage:

```c
#define STACK_PAINT_PATTERN  0xDEADBEEF

void eos_stack_paint(uint32_t *stack_bottom, size_t size_words) {
    for (size_t i = 0; i < size_words; i++) {
        stack_bottom[i] = STACK_PAINT_PATTERN;
    }
}

size_t eos_stack_watermark(const uint32_t *stack_bottom, size_t size_words) {
    size_t unused = 0;
    for (size_t i = 0; i < size_words; i++) {
        if (stack_bottom[i] != STACK_PAINT_PATTERN) break;
        unused++;
    }
    return unused * sizeof(uint32_t);
}

void stack_monitor_task(void *arg) {
    while (1) {
        size_t free = eos_stack_watermark(sensor_stack, SENSOR_STACK_WORDS);
        eos_log(EOS_LOG_INFO, "STACK",
                "sensor_read: %zu / %zu bytes free (%.0f%%)",
                free, SENSOR_STACK_WORDS * 4,
                100.0 * free / (SENSOR_STACK_WORDS * 4));
        eos_sleep_ms(5000);
    }
}
```

Output:

```
[  5.000] STACK : sensor_read: 1024 / 2048 bytes free (50%)
[ 10.000] STACK : sensor_read: 896 / 2048 bytes free (44%)
```

> ⚠️ If free drops below **25%**, increase the task stack size.

### 3. Runtime Stack Limit Checks

On ARMv8-M (Cortex-M33/M55) with PSPLIM register:

```c
__set_PSPLIM((uint32_t)stack_bottom + EOS_STACK_GUARD_SIZE);
```

On ARMv7-M (Cortex-M4), use the MPU guard page approach.

---

## Memory Leak Detection

### Host-Side: Valgrind

Build EoS for host (x86_64 Linux) and run under Valgrind:

```bash
cmake -B build-host -DEOS_BOARD=generic-x86_64 -DEOS_PRODUCT=iot -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
         build-host/eos-firmware
```

Example leak output:

```
==12345== 64 bytes in 1 blocks are definitely lost in loss record 1 of 3
==12345==    at 0x4C2BBAF: malloc (vg_replace_malloc.c:299)
==12345==    by 0x401234: eos_mqtt_connect (mqtt.c:87)
==12345==    by 0x401567: app_network_init (app.c:42)
==12345==    by 0x401012: main (main.c:18)
```

### CI: AddressSanitizer (ASAN)

```bash
cmake -B build-asan -DEOS_BOARD=generic-x86_64 -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ./build-asan/eos-firmware
```

ASAN catches: heap buffer overflow, use-after-free, double-free, stack buffer
overflow, global buffer overflow, and memory leaks.

### On-Target: Heap Tracking

For embedded targets, wrap `malloc`/`free`:

```c
#if EOS_DEBUG_HEAP
typedef struct {
    void *ptr; size_t size; const char *file; int line;
} eos_heap_record_t;

static eos_heap_record_t heap_log[256];
static int heap_log_count = 0;

void *eos_malloc_tracked(size_t size, const char *file, int line) {
    void *p = malloc(size);
    if (p && heap_log_count < 256) {
        heap_log[heap_log_count++] = (eos_heap_record_t){
            .ptr = p, .size = size, .file = file, .line = line
        };
    }
    return p;
}

void eos_free_tracked(void *ptr) {
    for (int i = 0; i < heap_log_count; i++) {
        if (heap_log[i].ptr == ptr) {
            heap_log[i] = heap_log[--heap_log_count];
            break;
        }
    }
    free(ptr);
}

void eos_heap_dump(void) {
    eos_log(EOS_LOG_WARN, "HEAP", "=== %d outstanding allocations ===",
            heap_log_count);
    for (int i = 0; i < heap_log_count; i++) {
        eos_log(EOS_LOG_WARN, "HEAP", "  %p: %zu bytes (%s:%d)",
                heap_log[i].ptr, heap_log[i].size,
                heap_log[i].file, heap_log[i].line);
    }
}

#define eos_malloc(sz)  eos_malloc_tracked((sz), __FILE__, __LINE__)
#define eos_free(p)     eos_free_tracked(p)
#endif
```

---

## Printf Debugging

### `eos_log()` — Structured Logging

```c
// Levels: EOS_LOG_TRACE, EOS_LOG_DEBUG, EOS_LOG_INFO,
//         EOS_LOG_WARN, EOS_LOG_ERROR, EOS_LOG_FATAL

eos_log(EOS_LOG_INFO,  "APP",  "Sensor initialized: %s", sensor_name);
eos_log(EOS_LOG_WARN,  "NET",  "MQTT reconnect attempt %d/%d", attempt, max);
eos_log(EOS_LOG_ERROR, "HAL",  "I2C NACK on address 0x%02X", addr);
```

Output:

```
[  1.234] INFO  APP   : Sensor initialized: BME280
[  5.001] WARN  NET   : MQTT reconnect attempt 2/5
[  5.002] ERROR HAL   : I2C NACK on address 0x76
```

Compile-time level filtering:

```bash
cmake -B build-stm32 ... -DEOS_LOG_LEVEL=EOS_LOG_WARN
```

### ITM / SWO Trace

ITM provides a high-speed debug channel through the SWO pin — no UART wiring
required.

**OpenOCD configuration:**

```bash
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "tpiu config internal /tmp/swo.log uart off 168000000 2000000"
```

**Firmware setup:**

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
ITM->LAR  = 0xC5ACCE55;
ITM->TER  = 0x1;
ITM->TCR  = (1 << 0) | (1 << 2) | (1 << 16);

void eos_itm_putchar(char c) {
    while (!(ITM->PORT[0].u32 & 1));
    ITM->PORT[0].u8 = (uint8_t)c;
}
```

**Read SWO output:**

```bash
telnet localhost 4444
> tpiu config internal - uart off 168000000 2000000
```

### Semihosting

Routes `printf()` through the debug probe. Very slow (~1 ms/char) but requires
no extra hardware.

```bash
# OpenOCD telnet:
arm semihosting enable
arm semihosting_fileio enable
```

```c
#include <stdio.h>
printf("Hello from semihosting!\n");
```

Link with semihosting specs:

```cmake
target_link_options(eos-firmware PRIVATE --specs=rdimon.specs)
```

> ⚠️ Use only for early boot debugging. Switch to UART or ITM for production.

---

## Common Pitfalls

### 1. Interrupt Priority Misconfiguration

On Cortex-M, **lower numeric priority = higher urgency**. STM32F4 uses 4
priority bits (values 0–15):

```c
// WRONG — USART2 (prio 1) preempts TIM2 (prio 4)
NVIC_SetPriority(USART2_IRQn, 1);
NVIC_SetPriority(TIM2_IRQn, 4);

// CORRECT — reserve low values for system
NVIC_SetPriority(SysTick_IRQn, 0);    // highest — kernel tick
NVIC_SetPriority(TIM2_IRQn, 2);       // high — timing critical
NVIC_SetPriority(USART2_IRQn, 5);     // medium — serial I/O
NVIC_SetPriority(I2C1_EV_IRQn, 8);    // low — sensor polling
```

**EoS rule:** Never set peripheral interrupts to priority 0–1.

### 2. DMA Buffer Alignment

DMA requires buffers aligned to transfer width:

```c
// WRONG — may be misaligned
uint8_t adc_buffer[256];

// CORRECT
__attribute__((aligned(4)))
uint8_t adc_buffer[256];

// Or use EoS macro
EOS_ALIGN(4) uint8_t adc_buffer[256];
```

On STM32H7 (Cortex-M7 with D-cache), DMA buffers must be non-cacheable or
cache must be invalidated:

```c
SCB_InvalidateDCache_by_Addr((uint32_t *)adc_buffer, sizeof(adc_buffer));
```

### 3. Missing `volatile` on Hardware Registers

```c
// WRONG — compiler may cache the read
uint32_t *status = (uint32_t *)0x40004800;
while (!(*status & (1 << 7)));  // infinite loop if optimized

// CORRECT
volatile uint32_t *status = (volatile uint32_t *)0x40004800;
while (!(*status & (1 << 7)));  // reads hardware every iteration
```

**Signs of missing volatile:**
- Code works at `-O0` but hangs at `-O2`
- Peripheral status bits never change
- Interrupt flags appear stuck

### 4. Shared Data Without Synchronization

```c
// WRONG — compiler may reorder or cache
volatile bool data_ready = false;
uint8_t shared_buffer[64];

void ISR_USART2(void) {
    fill_buffer(shared_buffer);
    data_ready = true;
}

void main_loop(void) {
    if (data_ready) {
        process(shared_buffer);  // may see stale data
        data_ready = false;
    }
}

// CORRECT — use memory barriers
void ISR_USART2(void) {
    fill_buffer(shared_buffer);
    __DMB();                     // data memory barrier
    data_ready = true;
}

void main_loop(void) {
    if (data_ready) {
        __DMB();
        process(shared_buffer);  // guaranteed fresh data
        data_ready = false;
    }
}
```

### 5. Printf in ISR Context

Never use `printf()` or `eos_log()` inside an ISR — they may block on UART TX,
causing missed interrupts or watchdog resets:

```c
// WRONG
void TIM2_IRQHandler(void) {
    eos_log(EOS_LOG_DEBUG, "TIM", "tick");  // blocks!
    TIM2->SR &= ~TIM_SR_UIF;
}

// CORRECT — use a flag and log from main context
static volatile uint32_t tim2_count = 0;

void TIM2_IRQHandler(void) {
    tim2_count++;
    TIM2->SR &= ~TIM_SR_UIF;
}

void main_loop(void) {
    static uint32_t last = 0;
    if (tim2_count != last) {
        eos_log(EOS_LOG_DEBUG, "TIM", "ticks: %lu", tim2_count);
        last = tim2_count;
    }
}
```

---

## Troubleshooting Quick Reference

| Symptom | Likely Cause | Debug Action |
|---------|-------------|--------------|
| Red LED on boot | Hard fault | Connect GDB, run `fault_regs` |
| Hang after `main()` | Stack overflow | Check watermarks, increase stack |
| Works at -O0, fails at -O2 | Missing `volatile` | Search for bare pointer casts |
| Random crashes under load | DMA alignment | Add `__attribute__((aligned(4)))` |
| UART output stops | ISR priority inversion | Audit NVIC priorities |
| GDB `target remote` fails | OpenOCD not running | Start OpenOCD first |
| `Cannot halt target` | SWD speed too high | Reduce `adapter speed` |
| Watchdog reset | Blocked main loop | Add `eos_wdt_feed()` calls |

---

## Next Steps

- [STM32 Deployment](stm32-deployment.md) — Full board bring-up tutorial
- [Wireless Stacks](wireless-stacks.md) — BLE and WiFi configuration
- [Networking Protocols](networking-protocols.md) — MQTT, HTTP, mDNS
- [Configuration Examples](configuration-examples.md) — Product profiles and build variants
