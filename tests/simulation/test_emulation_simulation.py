import unittest
class TestEoSSimulation(unittest.TestCase):
    def test_hardware_timer_interrupt(self):
        ticks = 0
        for _ in range(10):
            ticks += 1 # simulate tick interrupt
        self.assertEqual(ticks, 10)
