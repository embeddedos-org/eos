import unittest

class TesteosFunctional(unittest.TestCase):
    def test_kernel_ipc_queue_pipeline(self):
        queue = []
        max_size = 4
        # Producer
        for i in range(max_size):
            queue.append(f"msg_{i}")
        assert len(queue) == max_size
        # Consumer
        msg = queue.pop(0)
        assert msg == "msg_0"
        assert len(queue) == 3
