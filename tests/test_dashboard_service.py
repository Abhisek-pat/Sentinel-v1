import importlib
import json
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch


ROOT_DIR = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT_DIR))
app = importlib.import_module("dashboard_service.app")


class DashboardReasoningTest(unittest.TestCase):
    def test_reasoning_health_and_backlog_alert(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            kpi_dir = root / "kpi"
            database_path = root / "dashboard" / "kpi.db"
            kpi_dir.mkdir()
            now = int(time.time() * 1000)
            result = {
                "request_id": "request-1",
                "request_timestamp_ms": now - 2200,
                "timestamp_ms": now,
                "trigger": "loitering",
                "requested_provider": "openai-cloud",
                "provider": "openai-cloud",
                "model": "gpt-4.1-mini",
                "risk_level": "medium",
                "summary": "Loitering detected.",
                "recommended_action": "Review the track.",
                "latency_ms": 2109.46,
                "fallback_used": False,
                "primary_error": "",
                "success": True,
                "error": "",
            }
            (kpi_dir / "reasoning_results.jsonl").write_text(
                json.dumps(result) + "\n", encoding="utf-8"
            )
            (kpi_dir / "reasoning_requests.jsonl").write_text(
                "".join(
                    json.dumps({"request_id": f"pending-{index}"}) + "\n"
                    for index in range(4)
                ),
                encoding="utf-8",
            )
            (kpi_dir / "reasoning_requests.offset").write_text("0", encoding="utf-8")

            with (
                patch.object(app, "KPI_DIR", kpi_dir),
                patch.object(app, "DATABASE_PATH", database_path),
            ):
                app.initialize_database()
                app.ingest()
                connection = app.connect()
                try:
                    health = app.reasoning_health(connection, now - 60000)
                finally:
                    connection.close()

            self.assertEqual(1, health["requests"])
            self.assertEqual(100.0, health["success_rate_percent"])
            self.assertEqual(4, health["pending"])
            self.assertEqual("openai-cloud", health["providers"][0]["provider"])

            alerts = app.evaluate_alerts(
                {"capture_fps": 15, "source_fps": 15, "capture_delivery_percent": 100},
                1,
                {"throttled": "0x0"},
                {},
                health,
            )
            self.assertTrue(any(alert["code"] == "reasoning_backlog" for alert in alerts))

    def test_evaluation_results_are_ingested(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            kpi_dir = root / "kpi"
            database_path = root / "dashboard" / "kpi.db"
            kpi_dir.mkdir()
            result = {
                "evaluation_id": "evaluation-1",
                "timestamp_ms": int(time.time() * 1000),
                "label": "baseline",
                "provider": "openai-cloud",
                "model": "gpt-4.1-mini",
                "cases": 5,
                "iterations": 1,
                "runs": 5,
                "successes": 5,
                "correct_risk": 5,
                "success_rate_percent": 100.0,
                "risk_accuracy_percent": 100.0,
                "latency_avg_ms": 1143.11,
                "latency_min_ms": 1037.64,
                "latency_max_ms": 1324.29,
            }
            (kpi_dir / "evaluation_results.jsonl").write_text(
                json.dumps(result) + "\n", encoding="utf-8"
            )

            with (
                patch.object(app, "KPI_DIR", kpi_dir),
                patch.object(app, "DATABASE_PATH", database_path),
            ):
                app.initialize_database()
                counts = app.ingest()
                rows = app.evaluation_results()

            self.assertEqual(1, counts["evaluations"])
            self.assertEqual("openai-cloud", rows[0]["provider"])
            self.assertEqual(100.0, rows[0]["risk_accuracy_percent"])


if __name__ == "__main__":
    unittest.main()
