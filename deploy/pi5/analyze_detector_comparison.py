#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Any


METRICS = (
    "capture_fps_avg",
    "capture_fps_min",
    "detection_fps_avg",
    "inference_ms_avg",
    "inference_ms_max",
    "temperature_c_max",
    "rtsp_reconnect_delta",
    "throttled_samples",
)


def load_report(path: str) -> dict[str, Any]:
    return json.loads(Path(path).read_text(encoding="utf-8"))


def summary(report: dict[str, Any]) -> dict[str, Any]:
    return report.get("summary", {})


def number(value: Any) -> float | None:
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def delta(candidate: Any, baseline: Any) -> float | None:
    candidate_number = number(candidate)
    baseline_number = number(baseline)
    if candidate_number is None or baseline_number is None:
        return None
    return round(candidate_number - baseline_number, 3)


def percent_change(candidate: Any, baseline: Any) -> float | None:
    candidate_number = number(candidate)
    baseline_number = number(baseline)
    if candidate_number is None or baseline_number in (None, 0):
        return None
    return round(((candidate_number - baseline_number) / baseline_number) * 100, 2)


def compare(
    baseline_report: dict[str, Any],
    candidate_report: dict[str, Any],
    baseline_label: str,
    candidate_label: str,
    max_accuracy_proxy_drop_percent: float,
    min_latency_improvement_percent: float,
) -> dict[str, Any]:
    baseline_summary = summary(baseline_report)
    candidate_summary = summary(candidate_report)
    metrics = []
    for metric in METRICS:
        baseline_value = baseline_summary.get(metric)
        candidate_value = candidate_summary.get(metric)
        metrics.append(
            {
                "metric": metric,
                baseline_label: baseline_value,
                candidate_label: candidate_value,
                "delta": delta(candidate_value, baseline_value),
                "percent_change": percent_change(candidate_value, baseline_value),
            }
        )

    baseline_detection = number(baseline_summary.get("detection_fps_avg")) or 0
    candidate_detection = number(candidate_summary.get("detection_fps_avg")) or 0
    baseline_latency = number(baseline_summary.get("inference_ms_avg")) or 0
    candidate_latency = number(candidate_summary.get("inference_ms_avg")) or 0
    latency_improvement = (
        ((baseline_latency - candidate_latency) / baseline_latency) * 100
        if baseline_latency
        else 0
    )
    detection_drop = (
        ((baseline_detection - candidate_detection) / baseline_detection) * 100
        if baseline_detection
        else 0
    )

    gates = {
        "candidate_report_passed": bool(candidate_report.get("passed")),
        "latency_improvement_meets_threshold": latency_improvement
        >= min_latency_improvement_percent,
        "detection_fps_drop_within_threshold": detection_drop
        <= max_accuracy_proxy_drop_percent,
        "no_rtsp_reconnect_growth": (number(candidate_summary.get("rtsp_reconnect_delta")) or 0)
        <= 0,
        "no_throttling": (number(candidate_summary.get("throttled_samples")) or 0) == 0,
    }
    recommended = all(gates.values())
    return {
        "baseline_label": baseline_label,
        "candidate_label": candidate_label,
        "recommended_for_adoption": recommended,
        "latency_improvement_percent": round(latency_improvement, 2),
        "detection_fps_drop_percent": round(detection_drop, 2),
        "acceptance_gates": gates,
        "metrics": metrics,
    }


def render_markdown(result: dict[str, Any]) -> str:
    lines = [
        "# Sentinel Detector Comparison",
        "",
        f"Baseline: `{result['baseline_label']}`",
        f"Candidate: `{result['candidate_label']}`",
        f"Recommended for adoption: **{'YES' if result['recommended_for_adoption'] else 'NO'}**",
        "",
        "## Summary",
        "",
        "| Metric | Value |",
        "|---|---:|",
        f"| Latency improvement | {result['latency_improvement_percent']}% |",
        f"| Detection FPS drop | {result['detection_fps_drop_percent']}% |",
        "",
        "## Acceptance Gates",
        "",
        "| Gate | Result |",
        "|---|---:|",
    ]
    lines.extend(
        f"| {name} | {'PASS' if passed else 'FAIL'} |"
        for name, passed in result["acceptance_gates"].items()
    )
    lines.extend(
        [
            "",
            "## Metrics",
            "",
            "| Metric | Baseline | Candidate | Delta | Change |",
            "|---|---:|---:|---:|---:|",
        ]
    )
    for metric in result["metrics"]:
        lines.append(
            "| {metric} | {baseline} | {candidate} | {delta} | {change}% |".format(
                metric=metric["metric"],
                baseline=metric.get(result["baseline_label"], "--"),
                candidate=metric.get(result["candidate_label"], "--"),
                delta=metric.get("delta"),
                change=metric.get("percent_change"),
            )
        )
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare FP32 and INT8 detector soak reports.")
    parser.add_argument("--baseline-report", required=True)
    parser.add_argument("--candidate-report", required=True)
    parser.add_argument("--baseline-label", default="fp32")
    parser.add_argument("--candidate-label", default="int8")
    parser.add_argument("--json-output", default="")
    parser.add_argument("--markdown-output", default="")
    parser.add_argument("--max-detection-fps-drop-percent", type=float, default=5.0)
    parser.add_argument("--min-latency-improvement-percent", type=float, default=10.0)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    result = compare(
        load_report(args.baseline_report),
        load_report(args.candidate_report),
        args.baseline_label,
        args.candidate_label,
        args.max_detection_fps_drop_percent,
        args.min_latency_improvement_percent,
    )
    print(json.dumps(result, indent=2))
    if args.json_output:
        Path(args.json_output).write_text(json.dumps(result, indent=2), encoding="utf-8")
    if args.markdown_output:
        Path(args.markdown_output).write_text(render_markdown(result), encoding="utf-8")
    return 0 if result["recommended_for_adoption"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
