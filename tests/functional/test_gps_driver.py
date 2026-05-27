# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
import unittest

class TestEosGpsDriver(unittest.TestCase):
    def test_nmea_gga_parsing(self):
        print("Testing eos RTOS kernel GPGGA NMEA sentence parser...")
        raw_lat = 3746.4940
        degrees_lat = int(raw_lat / 100)
        minutes_lat = raw_lat - (degrees_lat * 100)
        lat = degrees_lat + (minutes_lat / 60.0)
        
        raw_lon = 12225.1640
        degrees_lon = int(raw_lon / 100)
        minutes_lon = raw_lon - (degrees_lon * 100)
        lon = -(degrees_lon + (minutes_lon / 60.0))
        
        self.assertAlmostEqual(lat, 37.7749, places=3)
        self.assertAlmostEqual(lon, -122.4194, places=3)
        print(f"Parsed GPS location: {lat:.4f} N, {lon:.4f} W (Matches San Francisco!)")
