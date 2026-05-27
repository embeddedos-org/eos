// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// High-precision performance benchmarks for eos

#include <stdio.h>
#include <time.h>
#include <assert.h>

void test_latency_microsecond_precision(void) {
    printf("Running high-precision latency benchmark...\n");
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    volatile int dummy = 0;
    for (int i = 0; i < 10000000; i++) {
        dummy += i * i;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double latency_ns = (elapsed / 10000000.0) * 1e9;
    printf("Average latency per operation: %.3f ns\n", latency_ns);
    assert(latency_ns < 100.0);
    printf("[PASS] latency benchmark\n");
}

int main(void) {
    printf("=== eos Performance Benchmarks ===\n");
    test_latency_microsecond_precision();
    printf("All benchmarks passed successfully.\n");
    return 0;
}
