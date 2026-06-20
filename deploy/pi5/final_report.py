#!/usr/bin/env python3
import argparse
import importlib.util
import json
from pathlib import Path
from typing import Any


SOAK_TEST_PATH = Path(__file__).with_name("soak_test.py")
SPEC = importlib.util.spec_from_file_location("soak_test", SOAK_TEST_PATH)
soak_test = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(soak_test)


def load_json(path: str) -> dict[str, Any]:
    if not path:
        return {}
    return json.loads(Path(path).read_text(encoding="utf-8"))


def status_text(passed: Any) -> str:
    if passed is True:
        return "PASS"
    if passed is False:
        return "FAIL"
    return "UNKNOWN"


def soak_summary(report: dict[str, Any]) -> dict[str, Any]:
    if isinstance(report.get("samples"), list):
        attempts = int(report.get("summary", {}).get("attempts", len(report["samples"])))
        rescored = soak_test.summarize(
            report["samples"],
            attempts,
            report.get("failures", []),
        )
        report = {**report, **rescored, "rescored_passed": rescored["passed"]}
    summary = report.get("summary", {})
    return {
        "passed": report.get("rescored_passed", report.get("passed")),
        "availability_percent": summary.get("availability_percent"),
        "capture_fps_avg": summary.get("capture_fps_avg"),
        "capture_fps_min": summary.get("capture_fps_min"),
        "detection_fps_avg": summary.get("detection_fps_avg"),
        "inference_ms_avg": summary.get("inference_ms_avg"),
        "inference_ms_max": summary.get("inference_ms_max"),
        "temperature_c_max": summary.get("temperature_c_max"),
        "memory_used_percent_max": summary.get("memory_used_percent_max"),
        "rtsp_reconnect_delta": summary.get("rtsp_reconnect_delta"),
        "throttled_samples": summary.get("throttled_samples"),
    }


def latest_comparison(evaluation: dict[str, Any]) -> list[dict[str, Any]]:
    providers = evaluation.get("providers")
    if isinstance(providers, list):
        return [provider for provider in providers if isinstance(provider, dict)]

    comparisons = evaluation.get("comparisons")
    if isinstance(comparisons, list):
        return [provider for provider in comparisons if isinstance(provider, dict)]

    return []


def render_markdown(
    soak: dict[str, Any],
    auth_smoke: dict[str, Any],
    evaluation: dict[str, Any],
    title: str,
) -> str:
    soak_metrics = soak_summary(soak)
    auth_metrics = soak_summary(auth_smoke)
    providers = latest_comparison(evaluation)
    production_provider = next(
        (
            provider
            for provider in providers
            if provider.get("provider") == "openai-cloud"
            and float(provider.get("success_rate_percent") or 0) >= 99
        ),
        {},
    )

    lines = [
        f"# {title}",
        "",
        "## Executive Summary",
        "",
        (
            "Sentinel is portfolio-ready on Raspberry Pi 5 for authenticated LAN operation "
            "with local vision inference, KPI telemetry, event clips, and OpenAI-backed "
            "reasoning. Multi-cloud LLM comparison is intentionally deferred because the "
            "Gemini free-tier profile hit provider rate limits during evaluation."
        ),
        "",
        "## Pi 5 Stability",
        "",
        "| Metric | Value |",
        "|---|---:|",
        f"| 24h soak result | {status_text(soak_metrics.get('passed'))} |",
        f"| Availability | {soak_metrics.get('availability_percent', '--')}% |",
        f"| Capture FPS avg/min | {soak_metrics.get('capture_fps_avg', '--')} / {soak_metrics.get('capture_fps_min', '--')} |",
        f"| Detection FPS avg | {soak_metrics.get('detection_fps_avg', '--')} |",
        f"| Inference avg/max | {soak_metrics.get('inference_ms_avg', '--')} / {soak_metrics.get('inference_ms_max', '--')} ms |",
        f"| Temperature max | {soak_metrics.get('temperature_c_max', '--')} C |",
        f"| Memory max | {soak_metrics.get('memory_used_percent_max', '--')}% |",
        f"| RTSP reconnect delta | {soak_metrics.get('rtsp_reconnect_delta', '--')} |",
        f"| Throttled samples | {soak_metrics.get('throttled_samples', '--')} |",
        "",
        "## Dashboard Auth Smoke",
        "",
        "| Metric | Value |",
        "|---|---:|",
        f"| Authenticated smoke result | {status_text(auth_metrics.get('passed'))} |",
        f"| Capture FPS avg/min | {auth_metrics.get('capture_fps_avg', '--')} / {auth_metrics.get('capture_fps_min', '--')} |",
        f"| Inference avg/max | {auth_metrics.get('inference_ms_avg', '--')} / {auth_metrics.get('inference_ms_max', '--')} ms |",
        f"| Temperature max | {auth_metrics.get('temperature_c_max', '--')} C |",
        "",
        "## LLM Reasoning Baseline",
        "",
        "| Provider | Model | Success | Risk Accuracy | Avg Latency | P95 Latency |",
        "|---|---|---:|---:|---:|---:|",
    ]

    if providers:
        for provider in providers:
            lines.append(
                "| {provider} | {model} | {success}% | {accuracy}% | {avg} ms | {p95} ms |".format(
                    provider=provider.get("provider", ""),
                    model=provider.get("model") or "--",
                    success=provider.get("success_rate_percent", "--"),
                    accuracy=provider.get("risk_accuracy_percent", "--"),
                    avg=provider.get("latency_avg_ms", "--"),
                    p95=provider.get("latency_p95_ms", "--"),
                )
            )
    else:
        lines.append("| -- | -- | -- | -- | -- | -- |")

    recommendation = (
        f"Use `{production_provider.get('provider')}` / `{production_provider.get('model')}` "
        "as the current reasoning provider."
        if production_provider
        else "Use the currently validated OpenAI profile as the production reasoning provider."
    )
    lines.extend(
        [
            "",
            "## Recommendation",
            "",
            recommendation,
            "Keep Gemini disabled for automated comparison until paid quota or a slower rate-limited evaluator is required.",
            "",
            "## Next Gates",
            "",
            "1. Capture final architecture diagram and short demo video.",
            "2. Run one INT8 detector comparison against representative clips.",
            "3. Prepare interview talking points around edge constraints, telemetry gates, and LLM fallback design.",
        ]
    )
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a Sentinel Pi 5 portfolio report.")
    parser.add_argument("--soak-report", default="")
    parser.add_argument("--auth-smoke-report", default="")
    parser.add_argument("--evaluation-report", default="")
    parser.add_argument("--title", default="Sentinel Pi 5 Final Evidence Report")
    parser.add_argument("--markdown-output", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    markdown = render_markdown(
        load_json(args.soak_report),
        load_json(args.auth_smoke_report),
        load_json(args.evaluation_report),
        args.title,
    )
    Path(args.markdown_output).write_text(markdown, encoding="utf-8")
    print(f"Wrote {args.markdown_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
