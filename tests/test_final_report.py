import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "final_report.py"
SPEC = importlib.util.spec_from_file_location("final_report", SCRIPT_PATH)
final_report = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(final_report)


class FinalReportTest(unittest.TestCase):
    def test_renders_key_evidence_sections(self) -> None:
        soak = {
            "passed": False,
            "rescored_passed": True,
            "summary": {
                "availability_percent": 100.0,
                "capture_fps_avg": 14.95,
                "capture_fps_min": 14.1,
                "detection_fps_avg": 4.98,
                "inference_ms_avg": 54.4,
                "inference_ms_max": 59.06,
                "temperature_c_max": 55.6,
                "memory_used_percent_max": 23.2,
                "rtsp_reconnect_delta": 0,
                "throttled_samples": 0,
            },
        }
        auth_smoke = {
            "passed": True,
            "summary": {
                "capture_fps_avg": 14.95,
                "capture_fps_min": 14.87,
                "inference_ms_avg": 54.18,
                "inference_ms_max": 55.79,
                "temperature_c_max": 49.6,
            },
        }
        evaluation = {
            "providers": [
                {
                    "provider": "openai-cloud",
                    "model": "gpt-4.1-mini",
                    "success_rate_percent": 100.0,
                    "risk_accuracy_percent": 100.0,
                    "latency_avg_ms": 1865.76,
                    "latency_p95_ms": 4838.77,
                }
            ]
        }

        markdown = final_report.render_markdown(
            soak,
            auth_smoke,
            evaluation,
            "Sentinel Evidence",
        )

        self.assertIn("# Sentinel Evidence", markdown)
        self.assertIn("24h soak result | PASS", markdown)
        self.assertIn("Authenticated smoke result | PASS", markdown)
        self.assertIn("openai-cloud", markdown)
        self.assertIn("Gemini disabled", markdown)

    def test_cli_writes_markdown(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.md"
            argv = [
                "final_report.py",
                "--markdown-output",
                str(output),
            ]
            with patch("sys.argv", argv):
                self.assertEqual(0, final_report.main())
            self.assertTrue(output.exists())

    def test_raw_soak_report_is_rescored(self) -> None:
        report = {
            "passed": False,
            "summary": {"attempts": 200},
            "failures": [],
            "samples": [],
        }
        healthy_sample = {
                    "capture_fps": 15,
                    "detection_fps": 5,
                    "inference_ms": 54,
                    "last_frame_age_ms": 2,
                    "temperature_c": 49,
                    "memory_used_percent": 22,
                    "disk_used_percent": 5,
                    "rtsp_reconnects": 0,
                    "throttled": "0x0",
                    "reasoning_pending": 0,
                    "dashboard_alerts": [],
                    "llm_status": "ok",
        }
        report["samples"] = [healthy_sample.copy() for _ in range(200)]
        report["samples"][50]["reasoning_pending"] = 10

        self.assertTrue(final_report.soak_summary(report)["passed"])


if __name__ == "__main__":
    unittest.main()
