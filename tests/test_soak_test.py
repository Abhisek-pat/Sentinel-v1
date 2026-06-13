import importlib.util
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "soak_test.py"
SPEC = importlib.util.spec_from_file_location("soak_test", SCRIPT_PATH)
soak_test = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(soak_test)


class SoakTestSummaryTest(unittest.TestCase):
    def test_healthy_samples_pass_acceptance_gates(self) -> None:
        samples = [
            {
                "capture_fps": 15.0,
                "detection_fps": 5.0,
                "inference_ms": 54.0,
                "last_frame_age_ms": 2,
                "rtsp_reconnects": 0,
                "temperature_c": 50.1,
                "throttled": "0x0",
                "memory_used_percent": 21.1,
                "disk_used_percent": 4.3,
                "reasoning_pending": 0,
                "dashboard_alerts": [],
                "llm_status": "ok",
            }
            for _ in range(10)
        ]
        result = soak_test.summarize(samples, 10, [])
        self.assertTrue(result["passed"])
        self.assertEqual(100.0, result["summary"]["availability_percent"])
        self.assertEqual(0, result["summary"]["rtsp_reconnect_delta"])

    def test_reconnect_growth_fails_acceptance(self) -> None:
        samples = [
            {
                "capture_fps": 15,
                "last_frame_age_ms": 2,
                "rtsp_reconnects": reconnects,
                "temperature_c": 50,
                "throttled": "0x0",
                "reasoning_pending": 0,
                "dashboard_alerts": [],
                "llm_status": "ok",
            }
            for reconnects in (0, 1)
        ]
        result = soak_test.summarize(samples, 2, [])
        self.assertFalse(result["passed"])
        self.assertFalse(result["acceptance_gates"]["no_rtsp_reconnect_growth"])


if __name__ == "__main__":
    unittest.main()
