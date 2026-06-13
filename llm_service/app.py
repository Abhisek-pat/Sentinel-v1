import json
import os
import threading
import statistics
import time
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

from providers import ProviderError, ReasoningResult, create_providers


class SceneRequest(BaseModel):
    scene_state: dict[str, Any]
    provider: str | None = None


class BenchmarkRequest(BaseModel):
    scene_state: dict[str, Any]
    providers: list[str] = Field(default_factory=list)
    iterations: int = Field(default=1, ge=1, le=20)


def result_payload(result: ReasoningResult, latency_ms: float) -> dict[str, Any]:
    return {
        "summary": result.summary,
        "risk_level": result.risk_level,
        "recommended_action": result.recommended_action,
        "provider": result.provider,
        "model": result.model,
        "latency_ms": round(latency_ms, 2),
        "success": True,
    }


app = FastAPI(title="Sentinel Reasoning Service", version="0.1.0")
providers = create_providers()
default_provider = os.environ.get("SENTINEL_LLM_PROVIDER", "mock")
KPI_DIR = Path(os.environ.get("SENTINEL_KPI_DIR", "/var/lib/sentinel/kpi"))
REQUESTS_PATH = KPI_DIR / "reasoning_requests.jsonl"
RESULTS_PATH = KPI_DIR / "reasoning_results.jsonl"
OFFSET_PATH = KPI_DIR / "reasoning_requests.offset"
POLL_INTERVAL_SEC = max(0.2, float(os.environ.get("SENTINEL_REASONING_POLL_SEC", "1")))
FALLBACK_PROVIDER = os.environ.get("SENTINEL_LLM_FALLBACK_PROVIDER", "mock")
worker_stop = threading.Event()
worker_thread: threading.Thread | None = None


def reason(scene_state: dict[str, Any], provider_name: str | None = None) -> dict[str, Any]:
    selected_name = provider_name or default_provider
    provider = providers.get(selected_name)
    if provider is None:
        raise HTTPException(
            status_code=400,
            detail=f"Unknown provider '{selected_name}'. Available: {sorted(providers)}",
        )

    started = time.perf_counter()
    try:
        result = provider.reason(scene_state)
    except ProviderError as error:
        raise HTTPException(status_code=502, detail=str(error)) from error
    return result_payload(result, (time.perf_counter() - started) * 1000.0)


def process_reasoning_request(request: dict[str, Any]) -> dict[str, Any]:
    requested_provider = str(request.get("provider") or default_provider)
    fallback_used = False
    primary_error = ""
    try:
        result = reason(request["scene_state"], requested_provider)
    except (HTTPException, KeyError) as primary_exception:
        primary_error_message = str(getattr(primary_exception, "detail", primary_exception))
        if requested_provider == FALLBACK_PROVIDER:
            result = {
                "summary": "Reasoning request failed.",
                "risk_level": "unknown",
                "recommended_action": "Review the reasoning service logs.",
                "provider": requested_provider,
                "model": "",
                "latency_ms": 0.0,
                "success": False,
                "error": primary_error_message,
            }
        else:
            try:
                result = reason(request["scene_state"], FALLBACK_PROVIDER)
                fallback_used = True
            except (HTTPException, KeyError) as fallback_error:
                result = {
                    "summary": "Reasoning request and fallback failed.",
                    "risk_level": "unknown",
                    "recommended_action": "Review the reasoning service logs.",
                    "provider": FALLBACK_PROVIDER,
                    "model": "",
                    "latency_ms": 0.0,
                    "success": False,
                    "error": str(getattr(fallback_error, "detail", fallback_error)),
                }
        primary_error = primary_error_message

    return {
        "request_id": str(request.get("request_id", "")),
        "request_timestamp_ms": int(request.get("timestamp_ms", 0)),
        "timestamp_ms": int(time.time() * 1000),
        "trigger": str(request.get("trigger", "unknown")),
        "requested_provider": requested_provider,
        "fallback_used": fallback_used,
        "primary_error": primary_error,
        "error": str(result.get("error", "")),
        **result,
    }


def process_pending_requests() -> int:
    if not REQUESTS_PATH.exists():
        return 0

    KPI_DIR.mkdir(parents=True, exist_ok=True)
    try:
        offset = int(OFFSET_PATH.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        offset = 0
    if offset > REQUESTS_PATH.stat().st_size:
        offset = 0

    processed = 0
    committed_offset = offset
    with REQUESTS_PATH.open("r", encoding="utf-8") as source:
        source.seek(offset)
        while True:
            line = source.readline()
            if not line:
                break
            line_end_offset = source.tell()
            try:
                request = json.loads(line)
                result = process_reasoning_request(request)
                with RESULTS_PATH.open("a", encoding="utf-8") as output:
                    output.write(json.dumps(result, separators=(",", ":")) + "\n")
                processed += 1
                committed_offset = line_end_offset
            except json.JSONDecodeError:
                if not line.endswith("\n"):
                    break
                committed_offset = line_end_offset
                continue
            except (HTTPException, OSError, TypeError, ValueError):
                continue

    OFFSET_PATH.write_text(str(committed_offset), encoding="utf-8")
    return processed


def worker_loop() -> None:
    while not worker_stop.wait(POLL_INTERVAL_SEC):
        process_pending_requests()


@app.on_event("startup")
def start_worker() -> None:
    global worker_thread
    KPI_DIR.mkdir(parents=True, exist_ok=True)
    worker_stop.clear()
    worker_thread = threading.Thread(target=worker_loop, name="sentinel-reasoning", daemon=True)
    worker_thread.start()


@app.on_event("shutdown")
def stop_worker() -> None:
    worker_stop.set()
    if worker_thread is not None:
        worker_thread.join(timeout=2.0)


@app.get("/health")
def health() -> dict[str, Any]:
    return {
        "status": "ok",
        "default_provider": default_provider,
        "providers": sorted(providers),
    }


@app.get("/providers")
def list_providers() -> dict[str, Any]:
    return {
        "default": default_provider,
        "providers": [
            {
                "name": name,
                "model": provider.model,
                "available": provider.available(),
                "status": "ready" if provider.available() else "configuration_error",
                "detail": (
                    "Configuration is ready; endpoint reachability is verified on request."
                    if provider.available()
                    else provider.configuration_error()
                ),
            }
            for name, provider in sorted(providers.items())
        ],
    }


@app.post("/reason")
def reason_over_scene(request: SceneRequest) -> dict[str, Any]:
    return reason(request.scene_state, request.provider)


@app.post("/benchmark")
def benchmark(request: BenchmarkRequest) -> dict[str, Any]:
    selected = request.providers or sorted(providers)
    runs: list[dict[str, Any]] = []

    for provider_name in selected:
        for iteration in range(request.iterations):
            try:
                run = reason(request.scene_state, provider_name)
                run["iteration"] = iteration + 1
                runs.append(run)
            except HTTPException as error:
                runs.append(
                    {
                        "provider": provider_name,
                        "iteration": iteration + 1,
                        "success": False,
                        "error": error.detail,
                    }
                )

    comparisons: list[dict[str, Any]] = []
    for provider_name in selected:
        provider_runs = [run for run in runs if run["provider"] == provider_name]
        successful_runs = [run for run in provider_runs if run.get("success")]
        latencies = [float(run["latency_ms"]) for run in successful_runs]
        risk_counts = {
            risk: sum(run.get("risk_level") == risk for run in successful_runs)
            for risk in ("low", "medium", "high")
        }
        comparisons.append(
            {
                "provider": provider_name,
                "model": successful_runs[0]["model"] if successful_runs else None,
                "runs": len(provider_runs),
                "successes": len(successful_runs),
                "success_rate_percent": round(
                    len(successful_runs) / max(1, len(provider_runs)) * 100.0, 2
                ),
                "latency_avg_ms": round(statistics.mean(latencies), 2) if latencies else None,
                "latency_min_ms": round(min(latencies), 2) if latencies else None,
                "latency_max_ms": round(max(latencies), 2) if latencies else None,
                "risk_counts": risk_counts,
            }
        )

    return {
        "scene_persons": len(request.scene_state.get("persons", [])),
        "runs": runs,
        "comparisons": comparisons,
    }
