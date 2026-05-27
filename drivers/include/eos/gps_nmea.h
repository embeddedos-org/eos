#ifndef EOS_GPS_NMEA_H
#define EOS_GPS_NMEA_H

typedef struct {
    double latitude;
    double longitude;
    float altitude;
    int sat_count;
    char timestamp[16];
    int fix_valid;
} eos_gps_data_t;

int eos_gps_parse_gga(const char *nmea, eos_gps_data_t *out_data);

#endif
