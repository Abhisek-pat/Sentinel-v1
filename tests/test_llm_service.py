import importlib
import json
import os
import sys
import threading
import tempfile
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest.mock import patch


SERVICE_DIR = Path(__file__).parents[1] / "llm_service"
sys.path.insert(0, str(SERVICE_DIR))
app = importlib.import_module("app")
providers_module = importlib.import_module("providers")


class CompatibleEndpointHandler(BaseHTTPRequestHandler):
    def do_POST(self) -> None:
        content_length = int(self.headers.get("Content-Length", "0"))
        request = json.loads(self.rfile.read(content_length))
        if request["model"] != "test-model":
            self.send_error(400)
            return

        response = json.dumps(
            {
                "choices": [
                    {
                        "message": {
                            "content": json.dumps(
                                {
                                    "summary": "Remote model evaluated the scene.",
                                    "risk_level": "low",
                                    "recommended_action": "Continue monitoring.",
                                }
                            )
                        }
                    }
                ]
            }
        ).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response)))
        self.end_headers()
        self.wfile.write(response)

    def log_message(self, format: str, *args: object) -> None:
        return


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
        self.assertEqual(1, len(result["comparisons"]))
        self.assertEqual(100.0, result["comparisons"][0]["success_rate_percent"])

    def test_evaluation_cases_cover_all_risk_levels(self) -> None:
        result = app.evaluation_cases()
        self.assertGreaterEqual(result["count"], 5)
        self.assertEqual(["high", "low", "medium"], result["risk_levels"])

    def test_mock_provider_scores_full_evaluation_accuracy(self) -> None:
        result = app.evaluate(app.EvaluationRequest(providers=["mock"], iterations=2))
        comparison = result["comparisons"][0]
        self.assertEqual(result["case_count"] * 2, result["total_requests"])
        self.assertEqual(100.0, comparison["success_rate_percent"])
        self.assertEqual(100.0, comparison["risk_accuracy_percent"])

    def test_openai_compatible_provider_contract(self) -> None:
        server = ThreadingHTTPServer(("127.0.0.1", 0), CompatibleEndpointHandler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            provider = providers_module.OpenAiCompatibleProvider(
                name="remote-test",
                base_url=f"http://127.0.0.1:{server.server_port}/v1",
                model="test-model",
            )
            result = provider.reason({"persons": [], "recent_events": []})
            self.assertEqual("remote-test", result.provider)
            self.assertEqual("test-model", result.model)
            self.assertEqual("low", result.risk_level)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_profiles_require_configured_credentials(self) -> None:
        profiles = json.dumps(
            [
                {
                    "name": "hosted-a",
                    "type": "openai_compatible",
                    "base_url": "http://127.0.0.1:9999/v1",
                    "model": "model-a",
                    "api_key_env": "SENTINEL_TEST_API_KEY",
                }
            ]
        )
        with patch.dict(
            os.environ,
            {"SENTINEL_LLM_PROFILES_JSON": profiles},
            clear=False,
        ):
            configured = providers_module.create_providers()
        self.assertIn("hosted-a", configured)
        self.assertFalse(configured["hosted-a"].available())
        self.assertIn("SENTINEL_TEST_API_KEY", configured["hosted-a"].configuration_error())

    def test_documentation_placeholders_are_not_available(self) -> None:
        provider = providers_module.OpenAiCompatibleProvider(
            name="model-a",
            base_url="http://MODEL-SERVER:PORT/v1",
            model="MODEL-NAME",
        )
        self.assertFalse(provider.available())
        self.assertIn("placeholder", provider.configuration_error())

    def test_provider_discovery_explains_readiness(self) -> None:
        result = app.list_providers()
        mock = next(provider for provider in result["providers"] if provider["name"] == "mock")
        self.assertEqual("ready", mock["status"])
        self.assertIn("reachability", mock["detail"])

    def test_async_queue_processes_each_request_once(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request_path = root / "reasoning_requests.jsonl"
            result_path = root / "reasoning_results.jsonl"
            offset_path = root / "reasoning_requests.offset"
            request_path.write_text(
                json.dumps(
                    {
                        "request_id": "request-1",
                        "timestamp_ms": 1000,
                        "trigger": "loitering",
                        "scene_state": {
                            "persons": [{"track_id": 7, "loitering": True}],
                            "recent_events": [],
                        },
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            with (
                patch.object(app, "KPI_DIR", root),
                patch.object(app, "REQUESTS_PATH", request_path),
                patch.object(app, "RESULTS_PATH", result_path),
                patch.object(app, "OFFSET_PATH", offset_path),
            ):
                self.assertEqual(1, app.process_pending_requests())
                self.assertEqual(0, app.process_pending_requests())

            result = json.loads(result_path.read_text(encoding="utf-8"))
            self.assertEqual("medium", result["risk_level"])
            self.assertEqual("loitering", result["trigger"])
            self.assertTrue(result["success"])

    def test_async_request_falls_back_to_mock(self) -> None:
        request = {
            "request_id": "request-2",
            "timestamp_ms": 1000,
            "trigger": "loitering",
            "provider": "missing-provider",
            "scene_state": {
                "persons": [{"track_id": 7, "loitering": True}],
                "recent_events": [],
            },
        }
        result = app.process_reasoning_request(request)
        self.assertTrue(result["fallback_used"])
        self.assertEqual("mock", result["provider"])
        self.assertEqual("medium", result["risk_level"])

    def test_async_queue_retries_partial_record(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            request_path = root / "reasoning_requests.jsonl"
            result_path = root / "reasoning_results.jsonl"
            offset_path = root / "reasoning_requests.offset"
            request_path.write_text('{"request_id":"partial"', encoding="utf-8")
            with (
                patch.object(app, "KPI_DIR", root),
                patch.object(app, "REQUESTS_PATH", request_path),
                patch.object(app, "RESULTS_PATH", result_path),
                patch.object(app, "OFFSET_PATH", offset_path),
            ):
                self.assertEqual(0, app.process_pending_requests())

            self.assertEqual("0", offset_path.read_text(encoding="utf-8"))
            self.assertFalse(result_path.exists())


if __name__ == "__main__":
    unittest.main()
