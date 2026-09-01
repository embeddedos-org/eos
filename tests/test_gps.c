// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "eos/gps_nmea.h"

static void test_gprmc_valid_fix(void) {
    const char *nmea =
        "$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,0.00,0.00,270526,,,A*7B";
    eos_gps_data_t data;

    assert(eos_gps_parse_gprmc(nmea, &data) == 0);
    assert(data.fix_valid);
    assert(fabs(data.latitude - 37.7749) < 0.01);
    assert(fabs(data.longitude - (-122.4194)) < 0.01);
    printf("[PASS] gps gprmc valid fix\n");
}

static void test_gprmc_void_fix(void) {
    const char *nmea =
        "$GPRMC,092750.000,V,3746.4949,N,12225.1644,W,0.00,0.00,270526,,,A*7B";
    eos_gps_data_t data;

    assert(eos_gps_parse_gprmc(nmea, &data) == 0);
    assert(!data.fix_valid);
    printf("[PASS] gps gprmc void fix\n");
}

static void test_gprmc_rejects_wrong_sentence(void) {
    eos_gps_data_t data;
    assert(eos_gps_parse_gprmc("$GPGGA,092750.000,3746.4949,N", &data) == -1);
    printf("[PASS] gps gprmc rejects non-GPRMC sentence\n");
}

static void test_gprmc_rejects_null_args(void) {
    eos_gps_data_t data;
    assert(eos_gps_parse_gprmc(NULL, &data) == -1);
    assert(eos_gps_parse_gprmc("$GPRMC,...", NULL) == -1);
    printf("[PASS] gps gprmc rejects null args\n");
}

static void test_gprmc_rejects_short_sentence(void) {
    eos_gps_data_t data;
    assert(eos_gps_parse_gprmc("$GPRMC,092750.000,A", &data) == -2);
    printf("[PASS] gps gprmc rejects short sentence\n");
}

int main(void) {
    test_gprmc_valid_fix();
    test_gprmc_void_fix();
    test_gprmc_rejects_wrong_sentence();
    test_gprmc_rejects_null_args();
    test_gprmc_rejects_short_sentence();
    printf("All GPS NMEA tests passed.\n");
    return 0;
}