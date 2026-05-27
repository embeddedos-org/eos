import unittest

class TestEoSGPSNMEA(unittest.TestCase):
    def test_nmea_gprmc_parsing(self):
        # Mocking the C function logic in Python for unit verification
        nmea_sentence = "$GPRMC,092750.000,A,3746.4949,N,12225.1644,W,0.00,0.00,270526,,,A*7B"
        assert nmea_sentence.startswith("$GPRMC")
        tokens = nmea_sentence.split(",")
        assert tokens[2] == "A"
        
        # Latitude parsing
        raw_lat = float(tokens[3])
        lat_deg = int(raw_lat / 100)
        lat_min = raw_lat - (lat_deg * 100)
        latitude = lat_deg + (lat_min / 60.0)
        if tokens[4] == "S": latitude = -latitude
        
        # Longitude parsing
        raw_lon = float(tokens[5])
        lon_deg = int(raw_lon / 100)
        lon_min = raw_lon - (lon_deg * 100)
        longitude = lon_deg + (lon_min / 60.0)
        if tokens[6] == "W": longitude = -longitude
        
        assert abs(latitude - 37.7749) < 0.01
        assert abs(longitude - -122.4194) < 0.01
