import os
import statistics
import time
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
