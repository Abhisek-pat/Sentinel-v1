#!/usr/bin/env python3
import argparse
import json
import statistics
import time
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def fetch_json(url: str, timeout_sec: float = 5.0) -> dict[str, Any]:
    with urllib.request.urlopen(url, timeout=timeout_sec) as response:
        return json.load(response)


def collect_sample(dashboard_url: str, llm_url: str) -> dict[str, Any]:
    dashboard = fetch_json(f"{dashboard_url}/api/dashboard?window_minutes=10")
    llm_health = fetch_json(f"{llm_url}/health")
    latest = dashboard.get("latest", {})
    device = dashboard.get("device", {})
    reasoning = dashboard.get("reasoning", {})
    return {
        "timestamp_ms": int(time.time() * 1000),
        "capture_fps": latest.get("capture_fps"),
        "detection_fps": latest.get("detection_fps"),
        "inference_ms": latest.get("inference_ms"),
        "last_frame_age_ms": latest.get("last_frame_age_ms"),
        "rtsp_reconnects": latest.get("rtsp_reconnects"),
        "temperature_c": device.get("temperature_c"),
        "throttled": device.get("throttled"),
        "memory_used_percent": device.get("memory_used_percent"),
        "disk_used_percent": device.get("disk_used_percent"),
        "reasoning_pending": reasoning.get("pending"),
        "dashboard_alerts": dashboard.get("alerts", []),
        "llm_status": llm_health.get("status"),
    }


def numeric_values(samples: list[dict[str, Any]], key: str) -> list[float]:
    return [float(sample[key]) for sample in samples if sample.get(key) is not None]


def summarize(
    samples: list[dict[str, Any]],
    attempts: int,
    failures: list[dict[str, Any]],
) -> dict[str, Any]:
    def metric(key: str, operation: str) -> float | None:
        values = numeric_values(samples, key)
        if not values:
            return None
        if operation == "min":
            return round(min(values), 2)
        if operation == "max":
            return round(max(values), 2)
        return round(statistics.mean(values), 2)

    reconnects = numeric_values(samples, "rtsp_reconnects")
    pending_values = numeric_values(samples, "reasoning_pending")
    backlog_warning_samples = sum(value > 3 for value in pending_values)
    backlog_warning_percent = round(
        backlog_warning_samples / max(1, len(samples)) * 100.0, 3
    )
    max_backlog_streak = 0
    current_backlog_streak = 0
    for value in pending_values:
        if value > 3:
            current_backlog_streak += 1
            max_backlog_streak = max(max_backlog_streak, current_backlog_streak)
        else:
            current_backlog_streak = 0
    throttled_samples = sum(
        sample.get("throttled") not in (None, "0x0") for sample in samples
    )
    llm_unhealthy_samples = sum(sample.get("llm_status") != "ok" for sample in samples)
    critical_alert_samples = sum(
        any(alert.get("severity") == "critical" for alert in sample.get("dashboard_alerts", []))
        for sample in samples
    )
    availability = round(len(samples) / max(1, attempts) * 100.0, 2)
    summary = {
        "attempts": attempts,
        "successful_samples": len(samples),
        "failed_samples": len(failures),
        "availability_percent": availability,
        "capture_fps_avg": metric("capture_fps", "avg"),
        "capture_fps_min": metric("capture_fps", "min"),
        "detection_fps_avg": metric("detection_fps", "avg"),
        "inference_ms_avg": metric("inference_ms", "avg"),
        "inference_ms_max": metric("inference_ms", "max"),
        "last_frame_age_ms_max": metric("last_frame_age_ms", "max"),
        "temperature_c_max": metric("temperature_c", "max"),
        "memory_used_percent_max": metric("memory_used_percent", "max"),
        "disk_used_percent_max": metric("disk_used_percent", "max"),
        "reasoning_pending_max": metric("reasoning_pending", "max"),
        "reasoning_backlog_warning_samples": backlog_warning_samples,
        "reasoning_backlog_warning_percent": backlog_warning_percent,
        "reasoning_backlog_max_consecutive_samples": max_backlog_streak,
        "rtsp_reconnect_delta": round(reconnects[-1] - reconnects[0], 2)
        if reconnects
        else None,
        "throttled_samples": throttled_samples,
        "llm_unhealthy_samples": llm_unhealthy_samples,
        "critical_alert_samples": critical_alert_samples,
    }
    gates = {
        "availability_at_least_99_percent": availability >= 99.0,
        "capture_fps_average_at_least_12": (summary["capture_fps_avg"] or 0) >= 12.0,
        "last_frame_age_below_1000_ms": (summary["last_frame_age_ms_max"] or 1e9) < 1000,
        "temperature_below_75_c": (summary["temperature_c_max"] or 1e9) < 75,
        "no_throttling": throttled_samples == 0,
        "no_rtsp_reconnect_growth": summary["rtsp_reconnect_delta"] == 0,
        "reasoning_backlog_not_sustained": (
            max_backlog_streak <= 3 and backlog_warning_percent <= 1.0
        ),
        "llm_service_healthy": llm_unhealthy_samples == 0,
        "no_critical_dashboard_alerts": critical_alert_samples == 0,
    }
    return {"summary": summary, "acceptance_gates": gates, "passed": all(gates.values())}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Collect Sentinel Pi soak-test evidence.")
    parser.add_argument("--duration-hours", type=float, default=24.0)
    parser.add_argument("--interval-sec", type=float, default=60.0)
    parser.add_argument("--samples", type=int, default=0, help="Stop after this many attempts.")
    parser.add_argument("--dashboard-url", default="http://127.0.0.1:8080")
    parser.add_argument("--llm-url", default="http://127.0.0.1:8090")
    parser.add_argument("--output", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    started = time.time()
    deadline = started + args.duration_hours * 3600
    attempts = 0
    samples: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    interrupted = False

    try:
        while (args.samples and attempts < args.samples) or (
            not args.samples and time.time() < deadline
        ):
            attempts += 1
            try:
                sample = collect_sample(args.dashboard_url, args.llm_url)
                samples.append(sample)
                print(
                    f"[{attempts}] capture={sample['capture_fps']} "
                    f"inference_ms={sample['inference_ms']} temp={sample['temperature_c']}"
                )
            except Exception as error:
                failures.append(
                    {"timestamp_ms": int(time.time() * 1000), "error": str(error)}
                )
                print(f"[{attempts}] sample failed: {error}")
            if (args.samples and attempts < args.samples) or (
                not args.samples and time.time() + args.interval_sec < deadline
            ):
                time.sleep(args.interval_sec)
    except KeyboardInterrupt:
        interrupted = True
        print("Soak test interrupted; writing a partial report.")

    result = {
        "started_at": datetime.fromtimestamp(started, timezone.utc).isoformat(),
        "finished_at": datetime.now(timezone.utc).isoformat(),
        "duration_sec": round(time.time() - started, 1),
        "interrupted": interrupted,
        **summarize(samples, attempts, failures),
        "failures": failures,
        "samples": samples,
    }
    output = Path(
        args.output
        or f"soak-report-{datetime.now().strftime('%Y%m%d-%H%M%S')}.json"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Soak report: {output}")
    print(f"Acceptance: {'PASS' if result['passed'] else 'FAIL'}")
    return 0 if result["passed"] and not interrupted else 1


if __name__ == "__main__":
    raise SystemExit(main())
