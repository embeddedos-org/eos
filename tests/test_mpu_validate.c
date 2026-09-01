// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project

/**
 * @file test_mpu_validate.c
 * @brief eos_mpu_validate() must reject regions the MPU cannot program
 *
 * ARMv7-M encodes a region's extent as a power-of-two exponent in RASR.SIZE
 * and requires RBAR's base to be aligned to that extent. A region violating
 * either cannot be expressed, so what gets programmed covers different memory
 * than was requested -- which for a task-isolation service is the failure the
 * validation exists to catch.
 *
 * Those two checks used to print WARN and leave the error count at zero, so
 * eos_mpu_validate() reported an unprogrammable configuration as clean.
 */

#include "eos/rtos_security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while (0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

/* One task with a single region, described by the caller. */
static int validate_one_region(uint32_t base, uint32_t size)
{
    EosMpuConfig m;
    eos_mpu_init(&m, 8);
    CHECK(eos_mpu_add_task(&m, "worker", 1, 0) == 0);
    CHECK(eos_mpu_add_region(&m, 0, "r", base, size,
                             EOS_MPU_RW, EOS_MPU_MEM_SRAM) == 0);
    return eos_mpu_validate(&m);
}

static void test_a_programmable_region_validates_clean(void)
{
    /* 4 KiB at a 4 KiB-aligned base: expressible, so no errors. */
    CHECK(validate_one_region(0x20001000, 0x1000) == 0);
    PASS("mpu: a programmable region validates clean");
}

static void test_non_power_of_two_size_is_an_error(void)
{
    /* 0x1800 is not 2^n; RASR.SIZE cannot encode it. */
    CHECK(validate_one_region(0x20000000, 0x1800) > 0);
    PASS("mpu: non-power-of-two size is an error");
}

static void test_misaligned_base_is_an_error(void)
{
    /* 4 KiB region based 0x100 into a page: RBAR cannot encode it. */
    CHECK(validate_one_region(0x20000100, 0x1000) > 0);
    PASS("mpu: base not aligned to size is an error");
}

/* `reg->base_addr % reg->size != 0 && reg->size > 0` divides before it checks:
 * && only guards what is to its right. UBSan aborts here; on x86 a zero
 * divisor is SIGFPE, while AArch64's udiv quietly yields 0 -- so this
 * reproduced on some hosts and not others. */
static void test_zero_size_is_an_error_and_does_not_divide_by_zero(void)
{
    CHECK(validate_one_region(0x20000000, 0) > 0);
    PASS("mpu: zero size is an error, with no division by zero");
}

static void test_size_below_the_hardware_minimum_is_an_error(void)
{
    /* 16 bytes is a power of two and aligned, but below the 32-byte floor. */
    CHECK(validate_one_region(0x20000000, 16) > 0);
    PASS("mpu: size below the 32-byte minimum is an error");
}

/* Errors accumulate rather than masking one another. */
static void test_a_task_over_its_hardware_region_budget_still_counts(void)
{
    EosMpuConfig m;
    eos_mpu_init(&m, 2);            /* only 2 hardware regions */
    CHECK(eos_mpu_add_task(&m, "worker", 1, 0) == 0);
    CHECK(eos_mpu_add_region(&m, 0, "ok", 0x20000000, 0x1000,
                             EOS_MPU_RW, EOS_MPU_MEM_SRAM) == 0);
    CHECK(eos_mpu_add_region(&m, 0, "bad", 0x20004000, 0x1800,
                             EOS_MPU_RW, EOS_MPU_MEM_SRAM) == 0);
    /* The unprogrammable size must be counted even though the region budget
     * is satisfied. */
    CHECK(eos_mpu_validate(&m) > 0);
    PASS("mpu: an unprogrammable region counts within the region budget");
}

int main(void)
{
    printf("=== EoS MPU Validation Tests ===\n");
    test_a_programmable_region_validates_clean();
    test_non_power_of_two_size_is_an_error();
    test_misaligned_base_is_an_error();
    test_zero_size_is_an_error_and_does_not_divide_by_zero();
    test_size_below_the_hardware_minimum_is_an_error();
    test_a_task_over_its_hardware_region_budget_still_counts();
    printf("=== ALL %d MPU TESTS PASSED ===\n", passed);
    return 0;
}
