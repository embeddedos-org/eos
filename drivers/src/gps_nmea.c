#include <eos/gps_nmea.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    double latitude;
    double longitude;
    float altitude;
    int sat_count;
    char timestamp[16];
    int fix_valid;
} eos_gps_data_t;

// Parse standard GPGGA NMEA sentence
int eos_gps_parse_gga(const char *nmea, eos_gps_data_t *out_data) {
    if (!nmea || !out_data) return -1;
    if (strncmp(nmea, "$GPGGA", 6) != 0) return -2; // Not a GPGGA sentence

    char buf[128];
    strncpy(buf, nmea, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tokens[20];
    int token_idx = 0;
    char *token = strtok(buf, ",");
    while (token && token_idx < 20) {
        tokens[token_idx++] = token;
        token = strtok(NULL, ",");
    }

    if (token_idx < 10) return -3; // Incomplete NMEA sentence

    // Time (UTC)
    strncpy(out_data->timestamp, tokens[1], sizeof(out_data->timestamp) - 1);

    // Latitude
    if (strlen(tokens[2]) > 0) {
        double raw_lat = atof(tokens[2]);
        int degrees = (int)(raw_lat / 100);
        double minutes = raw_lat - (degrees * 100);
        out_data->latitude = degrees + (minutes / 60.0);
        if (strcmp(tokens[3], "S") == 0) out_data->latitude = -out_data->latitude;
    }

    // Longitude
    if (strlen(tokens[4]) > 0) {
        double raw_lon = atof(tokens[4]);
        int degrees = (int)(raw_lon / 100);
        double minutes = raw_lon - (degrees * 100);
        out_data->longitude = degrees + (minutes / 60.0);
        if (strcmp(tokens[5], "W") == 0) out_data->longitude = -out_data->longitude;
    }

    // Fix Quality (0 = invalid, 1 = GPS fix, 2 = DGPS fix)
    out_data->fix_valid = atoi(tokens[6]);

    // Number of satellites
    out_data->sat_count = atoi(tokens[7]);

    // Altitude
    out_data->altitude = atof(tokens[9]);

    return 0;
}
