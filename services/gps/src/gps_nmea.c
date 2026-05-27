
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    double latitude;
    double longitude;
    float altitude;
    float speed_knots;
    int satellites;
    bool fix_valid;
} eos_gps_data_t;

/* Parse standard NMEA $GPRMC sentence */
int eos_gps_parse_gprmc(const char *nmea, eos_gps_data_t *data) {
    if (strncmp(nmea, "$GPRMC", 6) != 0) return -1;
    
    char buf[128];
    strncpy(buf, nmea, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    
    char *tokens[15];
    int tok_cnt = 0;
    char *tok = strtok(buf, ",");
    while (tok && tok_cnt < 15) {
        tokens[tok_cnt++] = tok;
        tok = strtok(NULL, ",");
    }
    
    if (tok_cnt < 8) return -2;
    
    /* Check status (A=Active, V=Void) */
    if (strcmp(tokens[2], "A") != 0) {
        data->fix_valid = false;
        return 0;
    }
    
    /* Parse Latitude: DDMM.MMMM */
    double raw_lat = atof(tokens[3]);
    int lat_deg = (int)(raw_lat / 100);
    double lat_min = raw_lat - (lat_deg * 100);
    data->latitude = lat_deg + (lat_min / 60.0);
    if (strcmp(tokens[4], "S") == 0) data->latitude = -data->latitude;
    
    /* Parse Longitude: DDDMM.MMMM */
    double raw_lon = atof(tokens[5]);
    int lon_deg = (int)(raw_lon / 100);
    double lon_min = raw_lon - (lon_deg * 100);
    data->longitude = lon_deg + (lon_min / 60.0);
    if (strcmp(tokens[6], "W") == 0) data->longitude = -data->longitude;
    
    data->speed_knots = atof(tokens[7]);
    data->fix_valid = true;
    return 0;
}
