import importlib.util
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path


SCRIPT_PATH = Path(__file__).parents[1] / "deploy" / "pi5" / "analyze_evaluations.py"
SPEC = importlib.util.spec_from_file_location("analyze_evaluations", SCRIPT_PATH)
analyze_evaluations = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(analyze_evaluations)


def sample_records() -> list[dict[str, object]]:
    return [
        {
            "evaluation_id": "eval-1",
            "timestamp_ms": 1000,
            "label": "baseline",
            "provider": "mock",
            "model": "sentinel-rules-v1",
            "cases": 15,
            "iterations": 1,
            "runs": 15,
            "successes": 15,
            "correct_risk": 15,
            "success_rate_percent": 100.0,
            "risk_accuracy_percent": 100.0,
            "latency_avg_ms": 0.0,
            "latency_p95_ms": 0.01,
            "latency_max_ms": 0.01,
            "per_risk_accuracy_json": "{\"low\":100.0,\"medium\":100.0,\"high\":100.0}",
            "confusion_json": "{}",
        },
        {
            "evaluation_id": "eval-1",
            "timestamp_ms": 1000,
            "label": "baseline",
            "provider": "openai-cloud",
            "model": "gpt-4.1-mini",
            "cases": 15,
            "iterations": 1,
            "runs": 15,
            "successes": 15,
            "correct_risk": 15,
            "success_rate_percent": 100.0,
            "risk_accuracy_percent": 100.0,
            "latency_avg_ms": 1338.12,
            "latency_p95_ms": 2470.84,
            "latency_max_ms": 2470.84,
            "per_risk_accuracy_json": "{\"low\":100.0,\"medium\":100.0,\"high\":100.0}",
            "confusion_json": "{}",
        },
    ]


class AnalyzeEvaluationsTest(unittest.TestCase):
    def test_normalizes_evaluate_response(self) -> None:
        payload = {
            "evaluation_id": "eval-2",
            "timestamp_ms": 2000,
            "label": "one-shot",
            "iterations": 1,
            "comparisons": sample_records(),
        }
        records = analyze_evaluations.normalize_records(payload)
        self.assertEqual(2, len(records))
        self.assertEqual("eval-2", records[0]["evaluation_id"])

    def test_ranks_by_accuracy_then_latency(self) -> None:
        comparison = analyze_evaluations.comparison_payload(sample_records())
        self.assertEqual("mock", comparison["providers"][0]["provider"])
        self.assertEqual("openai-cloud", comparison["providers"][1]["provider"])

    def test_selects_latest_evaluation_id(self) -> None:
        older = {**sample_records()[0], "evaluation_id": "old", "timestamp_ms": 1}
        newer = {**sample_records()[1], "evaluation_id": "new", "timestamp_ms": 2}
        selected = analyze_evaluations.select_records([older, newer])
        self.assertEqual("new", selected[0]["evaluation_id"])

    def test_renders_markdown(self) -> None:
        markdown = analyze_evaluations.render_markdown(
            analyze_evaluations.comparison_payload(sample_records())
        )
        self.assertIn("Sentinel LLM Model Comparison", markdown)
        self.assertIn("openai-cloud", markdown)
        self.assertIn("Per-Risk Accuracy", markdown)

    def test_cli_writes_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            input_path = root / "evaluations.json"
            json_path = root / "comparison.json"
            markdown_path = root / "comparison.md"
            input_path.write_text(json.dumps(sample_records()), encoding="utf-8")
            argv = sys.argv
            try:
                sys.argv = [
                    "analyze_evaluations.py",
                    str(input_path),
                    "--json-output",
                    str(json_path),
                    "--markdown-output",
                    str(markdown_path),
                ]
                with redirect_stdout(StringIO()):
                    self.assertEqual(0, analyze_evaluations.main())
            finally:
                sys.argv = argv
            self.assertTrue(json_path.exists())
            self.assertTrue(markdown_path.exists())


if __name__ == "__main__":
    unittest.main()
