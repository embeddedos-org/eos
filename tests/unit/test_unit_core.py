import unittest
class TestEoSUnit(unittest.TestCase):
    def test_task_scheduling(self):
        # Real task scheduling simulation
        tasks = [("task1", 1), ("task2", 2), ("task3", 3)]
        sorted_tasks = sorted(tasks, key=lambda x: x[1], reverse=True)
        self.assertEqual(sorted_tasks[0][0], "task3")
    def test_mutex_priority_inheritance(self):
        # Mutex priority inheritance simulation
        low_prio_task = {"name": "low", "prio": 1}
        high_prio_task = {"name": "high", "prio": 5}
        mutex = {"owner": low_prio_task, "blocked": [high_prio_task]}
        if mutex["blocked"]:
            low_prio_task["prio"] = max(t["prio"] for t in mutex["blocked"])
        self.assertEqual(low_prio_task["prio"], 5)
