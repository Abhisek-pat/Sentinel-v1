#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Any


def normalize_records(payload: Any) -> list[dict[str, Any]]:
    if isinstance(payload, list):
        return [record for record in payload if isinstance(record, dict)]

    if isinstance(payload, dict) and isinstance(payload.get("comparisons"), list):
        records: list[dict[str, Any]] = []
        for comparison in payload["comparisons"]:
            if not isinstance(comparison, dict):
                continue
            records.append(
                {
                    **comparison,
                    "evaluation_id": payload.get("evaluation_id", ""),
                    "timestamp_ms": payload.get("timestamp_ms", 0),
                    "label": payload.get("label", ""),
                    "iterations": payload.get("iterations", 1),
                }
            )
        return records

    raise ValueError("Expected /api/evaluations list or /evaluate response object.")


def select_records(
    records: list[dict[str, Any]],
    evaluation_id: str = "",
    label: str = "",
) -> list[dict[str, Any]]:
    if evaluation_id:
        selected = [record for record in records if record.get("evaluation_id") == evaluation_id]
    elif label:
        selected = [record for record in records if record.get("label") == label]
    else:
        latest_id = max(
            (str(record.get("evaluation_id", "")) for record in records),
            key=lambda value: max(
                (
                    int(record.get("timestamp_ms") or 0)
                    for record in records
                    if str(record.get("evaluation_id", "")) == value
                ),
                default=0,
            ),
            default="",
        )
        selected = [record for record in records if str(record.get("evaluation_id", "")) == latest_id]

    if not selected:
        raise ValueError("No evaluation records matched the requested selection.")
    return selected


def parse_json_field(value: Any) -> dict[str, Any]:
    if isinstance(value, dict):
        return value
    if isinstance(value, str) and value:
        try:
            parsed = json.loads(value)
            return parsed if isinstance(parsed, dict) else {}
        except json.JSONDecodeError:
            return {}
    return {}


def score_record(record: dict[str, Any]) -> tuple[float, float, float, float]:
    accuracy = float(record.get("risk_accuracy_percent") or 0)
    success = float(record.get("success_rate_percent") or 0)
    p95 = float(record.get("latency_p95_ms") or record.get("latency_max_ms") or 1e9)
    avg = float(record.get("latency_avg_ms") or 1e9)
    return (-accuracy, -success, p95, avg)


def ranked_records(records: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return sorted(records, key=score_record)


def comparison_payload(records: list[dict[str, Any]]) -> dict[str, Any]:
    ranked = ranked_records(records)
    return {
        "evaluation_id": records[0].get("evaluation_id", ""),
        "label": records[0].get("label", ""),
        "providers": [
            {
                "rank": index + 1,
                "provider": record.get("provider", ""),
                "model": record.get("model", ""),
                "cases": record.get("cases"),
                "iterations": record.get("iterations"),
                "runs": record.get("runs"),
                "successes": record.get("successes"),
                "correct_risk": record.get("correct_risk"),
                "success_rate_percent": record.get("success_rate_percent"),
                "risk_accuracy_percent": record.get("risk_accuracy_percent"),
                "latency_avg_ms": record.get("latency_avg_ms"),
                "latency_p95_ms": record.get("latency_p95_ms"),
                "latency_max_ms": record.get("latency_max_ms"),
                "per_risk_accuracy": parse_json_field(
                    record.get("per_risk_accuracy_json")
                    or record.get("per_risk_accuracy")
                ),
                "confusion": parse_json_field(
                    record.get("confusion_json") or record.get("confusion")
                ),
            }
            for index, record in enumerate(ranked)
        ],
    }


def render_markdown(payload: dict[str, Any]) -> str:
    lines = [
        "# Sentinel LLM Model Comparison",
        "",
        f"Evaluation: `{payload.get('evaluation_id', '')}`",
        f"Label: `{payload.get('label', '')}`",
        "",
        "## Ranking",
        "",
        "| Rank | Provider | Model | Cases x Iterations | Success | Risk Accuracy | Avg Latency | P95 Latency |",
        "|---:|---|---|---:|---:|---:|---:|---:|",
    ]
    for provider in payload["providers"]:
        lines.append(
            "| {rank} | {provider} | {model} | {cases} x {iterations} | "
            "{success_rate_percent}% | {risk_accuracy_percent}% | "
            "{latency_avg_ms} ms | {latency_p95_ms} ms |".format(**provider)
        )

    lines.extend(["", "## Per-Risk Accuracy", "", "| Provider | Low | Medium | High |", "|---|---:|---:|---:|"])
    for provider in payload["providers"]:
        risk = provider.get("per_risk_accuracy") or {}
        lines.append(
            "| {provider} | {low} | {medium} | {high} |".format(
                provider=provider["provider"],
                low=risk.get("low", "--"),
                medium=risk.get("medium", "--"),
                high=risk.get("high", "--"),
            )
        )

    best = payload["providers"][0] if payload["providers"] else {}
    lines.extend(
        [
            "",
            "## Recommendation",
            "",
            (
                f"Use `{best.get('provider', '')}` as the current comparison winner "
                f"for this labeled suite. Re-run the report after adding local or LAN providers."
            ),
        ]
    )
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze Sentinel LLM evaluation results.")
    parser.add_argument("input", help="JSON file from /api/evaluations or /evaluate.")
    parser.add_argument("--evaluation-id", default="")
    parser.add_argument("--label", default="")
    parser.add_argument("--json-output", default="")
    parser.add_argument("--markdown-output", default="")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    payload = json.loads(Path(args.input).read_text(encoding="utf-8"))
    records = select_records(normalize_records(payload), args.evaluation_id, args.label)
    comparison = comparison_payload(records)
    print(json.dumps(comparison, indent=2))
    if args.json_output:
        Path(args.json_output).write_text(json.dumps(comparison, indent=2), encoding="utf-8")
    if args.markdown_output:
        Path(args.markdown_output).write_text(render_markdown(comparison), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
