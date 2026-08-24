import unittest

def eos_gps_parse_gprmc(nmea):
    if nmea is None:
        return -1, None
    if not nmea.startswith("$GPRMC"):
        return -1, None

    # Preserves empty tokens according to NMEA 0183 standard (strchr comma splitting)
    tokens = nmea.split(",")
    if len(tokens) < 8:
        return -2, None

    data = {
        "latitude": 0.0,
        "longitude": 0.0,
        "speed_knots": 0.0,
        "fix_valid": False,
    }

    # Status: A=Active, V=Void
    if tokens[2] != "A":
        data["fix_valid"] = False
        return 0, data

    # Parse Latitude: DDMM.MMMM
    if len(tokens[3]) > 0:
        raw_lat = float(tokens[3])
        lat_deg = int(raw_lat / 100)
        lat_min = raw_lat - (lat_deg * 100)
        data["latitude"] = lat_deg + (lat_min / 60.0)
        if len(tokens) > 4 and tokens[4] == "S":
            data["latitude"] = -data["latitude"]

    # Parse Longitude: DDDMM.MMMM
    if len(tokens[5]) > 0:
        raw_lon = float(tokens[5])
        lon_deg = int(raw_lon / 100)
        lon_min = raw_lon - (lon_deg * 100)
        data["longitude"] = lon_deg + (lon_min / 60.0)
        if len(tokens) > 6 and tokens[6] == "W":
            data["longitude"] = -data["longitude"]

    # Parse Speed
    if len(tokens[7]) > 0:
        data["speed_knots"] = float(tokens[7])

    data["fix_valid"] = True
    return 0, data

class TestEoSGPSNMEA(unittest.TestCase):
    def test_nmea_gprmc_parsing(self):
        nmea_sentence = "$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,0.00,0.00,270526,,,A*7B"
        rc, data = eos_gps_parse_gprmc(nmea_sentence)
        self.assertEqual(rc, 0)
        self.assertTrue(data["fix_valid"])
        self.assertAlmostEqual(data["latitude"], 37.7749, places=3)
        self.assertAlmostEqual(data["longitude"], -122.4194, places=3)
        self.assertAlmostEqual(data["speed_knots"], 0.0, places=2)

    def test_nmea_gprmc_void_fix_empty_fields(self):
        # Regression test: standard NMEA sentence with Void fix status and empty coordinate fields
        nmea_sentence = "$GPRMC,083559.00,V,,,,,,,270526,,,N*4D"
        rc, data = eos_gps_parse_gprmc(nmea_sentence)
        self.assertEqual(rc, 0)
        self.assertFalse(data["fix_valid"])
        self.assertEqual(data["latitude"], 0.0)
        self.assertEqual(data["longitude"], 0.0)

    def test_nmea_gprmc_empty_speed_field(self):
        # Regression test: active fix with omitted speed and course fields
        nmea_sentence = "$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,,,270526,,,A*7B"
        rc, data = eos_gps_parse_gprmc(nmea_sentence)
        self.assertEqual(rc, 0)
        self.assertTrue(data["fix_valid"])
        self.assertAlmostEqual(data["latitude"], 37.7749, places=3)
        self.assertAlmostEqual(data["longitude"], -122.4194, places=3)
        self.assertEqual(data["speed_knots"], 0.0)

    def test_nmea_gprmc_invalid_sentence(self):
        rc, data = eos_gps_parse_gprmc(None)
        self.assertEqual(rc, -1)

        rc, data = eos_gps_parse_gprmc("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47")
        self.assertEqual(rc, -1)

        rc, data = eos_gps_parse_gprmc("$GPRMC,short")
        self.assertEqual(rc, -2)

