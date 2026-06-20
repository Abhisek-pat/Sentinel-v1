import importlib.util
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "analyze_detector_comparison.py"
SPEC = importlib.util.spec_from_file_location("analyze_detector_comparison", SCRIPT_PATH)
analyze_detector_comparison = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(analyze_detector_comparison)


def report(inference_ms: float, detection_fps: float, passed: bool = True) -> dict:
    return {
        "passed": passed,
        "summary": {
            "capture_fps_avg": 15.0,
            "capture_fps_min": 14.8,
            "detection_fps_avg": detection_fps,
            "inference_ms_avg": inference_ms,
            "inference_ms_max": inference_ms + 5,
            "temperature_c_max": 55.0,
            "rtsp_reconnect_delta": 0,
            "throttled_samples": 0,
        },
    }


class DetectorComparisonTest(unittest.TestCase):
    def test_recommends_candidate_with_latency_gain_and_stable_detection(self) -> None:
        result = analyze_detector_comparison.compare(
            report(54.0, 5.0),
            report(42.0, 4.9),
            "fp32",
            "int8",
            max_accuracy_proxy_drop_percent=5,
            min_latency_improvement_percent=10,
        )

        self.assertTrue(result["recommended_for_adoption"])
        self.assertGreater(result["latency_improvement_percent"], 10)

    def test_rejects_candidate_when_detection_proxy_drops_too_much(self) -> None:
        result = analyze_detector_comparison.compare(
            report(54.0, 5.0),
            report(42.0, 4.0),
            "fp32",
            "int8",
            max_accuracy_proxy_drop_percent=5,
            min_latency_improvement_percent=10,
        )

        self.assertFalse(result["recommended_for_adoption"])
        self.assertFalse(result["acceptance_gates"]["detection_fps_drop_within_threshold"])

    def test_markdown_contains_recommendation(self) -> None:
        result = analyze_detector_comparison.compare(
            report(54.0, 5.0),
            report(42.0, 4.9),
            "fp32",
            "int8",
            max_accuracy_proxy_drop_percent=5,
            min_latency_improvement_percent=10,
        )

        markdown = analyze_detector_comparison.render_markdown(result)
        self.assertIn("Recommended for adoption: **YES**", markdown)


if __name__ == "__main__":
    unittest.main()
