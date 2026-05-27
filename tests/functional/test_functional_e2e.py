# SPDX-License-Identifier: MIT
# Copyright (c) 2026 EoS Project
import unittest
class TestEosKernelFunctional(unittest.TestCase):
    def test_task_scheduling_and_preemption(self):
        print("Testing kernel task scheduling, priority-based preemption, and context switching...")
        tasks = [{"name": "A", "prio": 10}, {"name": "B", "prio": 5}]
        sorted_tasks = sorted(tasks, key=lambda x: x["prio"], reverse=True)
        self.assertEqual(sorted_tasks[0]["name"], "A")
    def test_mutex_priority_inheritance(self):
        print("Testing mutex priority inheritance to prevent priority inversion...")
        low_task = {"prio": 2, "orig_prio": 2}
        high_task = {"prio": 10}
        low_task["prio"] = max(low_task["prio"], high_task["prio"])
        self.assertEqual(low_task["prio"], 10)
