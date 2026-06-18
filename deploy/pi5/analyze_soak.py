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


def render_markdown(report: dict[str, Any], rescored: dict[str, Any]) -> str:
    summary = rescored["summary"]
    gates = rescored["acceptance_gates"]
    rows = [
        ("Duration", f"{report.get('duration_sec', 0) / 3600:.1f} h"),
        ("Availability", f"{summary.get('availability_percent')}%"),
        ("Samples", str(summary.get("successful_samples"))),
        ("Failures", str(summary.get("failed_samples"))),
        ("Capture FPS avg/min", f"{summary.get('capture_fps_avg')} / {summary.get('capture_fps_min')}"),
        ("Detection FPS avg", str(summary.get("detection_fps_avg"))),
        ("Inference avg/max", f"{summary.get('inference_ms_avg')} / {summary.get('inference_ms_max')} ms"),
        ("Last frame age max", f"{summary.get('last_frame_age_ms_max')} ms"),
        ("Temperature max", f"{summary.get('temperature_c_max')} C"),
        ("Memory max", f"{summary.get('memory_used_percent_max')}%"),
        ("Disk max", f"{summary.get('disk_used_percent_max')}%"),
        ("RTSP reconnect delta", str(summary.get("rtsp_reconnect_delta"))),
        ("Throttled samples", str(summary.get("throttled_samples"))),
        ("Reasoning pending max", str(summary.get("reasoning_pending_max"))),
        (
            "Reasoning backlog warning samples",
            str(summary.get("reasoning_backlog_warning_samples")),
        ),
        (
            "Reasoning backlog max streak",
            str(summary.get("reasoning_backlog_max_consecutive_samples")),
        ),
    ]
    lines = [
        "# Sentinel Pi 5 Soak-Test Analysis",
        "",
        f"Started: `{report.get('started_at')}`",
        f"Finished: `{report.get('finished_at')}`",
        f"Result: **{'PASS' if rescored['passed'] else 'FAIL'}**",
        "",
        "## Summary",
        "",
        "| Metric | Value |",
        "|---|---:|",
    ]
    lines.extend(f"| {name} | {value} |" for name, value in rows)
    lines.extend(["", "## Acceptance Gates", "", "| Gate | Result |", "|---|---:|"])
    lines.extend(f"| {name} | {'PASS' if value else 'FAIL'} |" for name, value in gates.items())
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze an existing Sentinel soak report.")
    parser.add_argument("report", help="Path to pi5-24h-soak.json")
    parser.add_argument("--markdown-output", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_path = Path(args.report)
    report = json.loads(report_path.read_text(encoding="utf-8"))
    rescored = soak_test.summarize(
        report.get("samples", []),
        int(report.get("summary", {}).get("attempts", len(report.get("samples", [])))),
        report.get("failures", []),
    )
    output = {
        "original_passed": report.get("passed"),
        "rescored_passed": rescored["passed"],
        **rescored,
    }
    print(json.dumps(output, indent=2))
    if args.markdown_output:
        Path(args.markdown_output).write_text(render_markdown(report, rescored), encoding="utf-8")
    return 0 if rescored["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
