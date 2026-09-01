// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

/**
 * @file gps_nmea.h
 * @brief EoS GPS Service — NMEA 0183 sentence parsing
 *
 * Provides parsing of standard NMEA GPRMC sentences into a structured
 * position/velocity fix usable by higher-level location services.
 */

#ifndef EOS_GPS_NMEA_H
#define EOS_GPS_NMEA_H

#include <stdbool.h>
#include <eos/eos_config.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double latitude;
    double longitude;
    float altitude;
    float speed_knots;
    int satellites;
    bool fix_valid;
} eos_gps_data_t;

/**
 * Parse a standard NMEA $GPRMC sentence.
 *
 * @param nmea NUL-terminated NMEA sentence, e.g.
 *             "$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,0.00,0.00,270526,,,A*7B"
 * @param data Output struct populated on success.
 * @return 0 on success (data->fix_valid indicates whether the fix is valid),
 *         -1 on invalid arguments or sentence type, -2 if the sentence has
 *         too few fields to parse.
 */
int eos_gps_parse_gprmc(const char *nmea, eos_gps_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* EOS_GPS_NMEA_H */