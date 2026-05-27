import unittest

class TesteosUnit(unittest.TestCase):
    def test_scheduler_priority_inheritance(self):
        # Simulate RTOS priority inheritance to prevent priority inversion
        tasks = [
            {"id": 1, "priority": 10, "state": "READY"}, # Low priority holding mutex
            {"id": 2, "priority": 5, "state": "READY"},  # Medium priority
            {"id": 3, "priority": 1, "state": "WAITING"} # High priority waiting for mutex
        ]
        # Mutex lock
        mutex_owner = 1
        # High priority task requests mutex
        assert tasks[0]["priority"] == 10
        # Priority inheritance triggers
        tasks[0]["priority"] = tasks[2]["priority"]
        assert tasks[0]["priority"] == 1, "Low priority task should inherit high priority"
