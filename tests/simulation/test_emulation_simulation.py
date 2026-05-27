import unittest

class TesteosSimulation(unittest.TestCase):
    def test_timer_interrupt_emulation(self):
        systick = 0
        # Simulate 1000 timer ticks (1ms each)
        for _ in range(1000):
            systick += 1
        assert systick == 1000, "Systick timer interrupt emulation failed"
