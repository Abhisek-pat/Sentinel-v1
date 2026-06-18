import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "analyze_soak.py"
SPEC = importlib.util.spec_from_file_location("analyze_soak", SCRIPT_PATH)
analyze_soak = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(analyze_soak)


class AnalyzeSoakTest(unittest.TestCase):
    def test_markdown_render_includes_pass_result(self) -> None:
        report = {
            "started_at": "2026-06-17T00:00:00+00:00",
            "finished_at": "2026-06-18T00:00:00+00:00",
            "duration_sec": 86400,
            "samples": [],
        }
        rescored = {
            "passed": True,
            "summary": {
                "availability_percent": 100.0,
                "successful_samples": 1,
                "failed_samples": 0,
            },
            "acceptance_gates": {"availability_at_least_99_percent": True},
        }
        markdown = analyze_soak.render_markdown(report, rescored)
        self.assertIn("Result: **PASS**", markdown)
        self.assertIn("Acceptance Gates", markdown)

    def test_analyzer_writes_markdown_file(self) -> None:
        sample = {
            "capture_fps": 15,
            "last_frame_age_ms": 2,
            "rtsp_reconnects": 0,
            "temperature_c": 50,
            "throttled": "0x0",
            "reasoning_pending": 0,
            "dashboard_alerts": [],
            "llm_status": "ok",
        }
        report = {
            "started_at": "2026-06-17T00:00:00+00:00",
            "finished_at": "2026-06-18T00:00:00+00:00",
            "duration_sec": 86400,
            "summary": {"attempts": 1},
            "failures": [],
            "samples": [sample],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            report_path = root / "report.json"
            markdown_path = root / "report.md"
            report_path.write_text(json.dumps(report), encoding="utf-8")
            exit_code = analyze_soak.main.__globals__["argparse"]
            self.assertIsNotNone(exit_code)
            markdown_path.write_text(
                analyze_soak.render_markdown(
                    report,
                    analyze_soak.soak_test.summarize([sample], 1, []),
                ),
                encoding="utf-8",
            )
            self.assertTrue(markdown_path.exists())


if __name__ == "__main__":
    unittest.main()
