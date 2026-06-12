import importlib
import sys
import unittest
from pathlib import Path


SERVICE_DIR = Path(__file__).parents[1] / "llm_service"
sys.path.insert(0, str(SERVICE_DIR))
app = importlib.import_module("app")


class LlmServiceTest(unittest.TestCase):
    def test_empty_scene_is_low_risk(self) -> None:
        result = app.reason({"persons": [], "recent_events": []})
        self.assertEqual("low", result["risk_level"])
        self.assertEqual("mock", result["provider"])
        self.assertTrue(result["success"])

    def test_loitering_scene_is_medium_risk(self) -> None:
        result = app.reason(
            {
                "persons": [{"track_id": 7, "loitering": True}],
                "recent_events": [],
            }
        )
        self.assertEqual("medium", result["risk_level"])

    def test_high_risk_event_is_high_risk(self) -> None:
        result = app.reason(
            {
                "persons": [],
                "recent_events": ["Emergency forced entry reported"],
            }
        )
        self.assertEqual("high", result["risk_level"])

    def test_benchmark_runs_requested_iterations(self) -> None:
        result = app.benchmark(
            app.BenchmarkRequest(
                scene_state={"persons": [], "recent_events": []},
                providers=["mock"],
                iterations=3,
            )
        )
        self.assertEqual(3, len(result["runs"]))
        self.assertTrue(all(run["success"] for run in result["runs"]))


if __name__ == "__main__":
    unittest.main()
