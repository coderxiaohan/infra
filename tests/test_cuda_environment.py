import unittest
from pathlib import Path

from scripts.check_cuda_environment import collect


class CudaEnvironmentTest(unittest.TestCase):
    def test_collect_reports_stable_schema_without_cmake_probe(self):
        payload = collect(Path("."), try_cmake=False)

        self.assertIn("cuda_available", payload)
        self.assertIn("tools", payload)
        self.assertIn("nvcc", payload)
        self.assertIn("nvidia_smi", payload)
        self.assertFalse(payload["cmake_cuda_configure"]["attempted"])


if __name__ == "__main__":
    unittest.main()
