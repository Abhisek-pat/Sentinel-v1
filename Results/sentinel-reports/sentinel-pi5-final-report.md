# Sentinel Pi 5 Final Evidence Report

## Executive Summary

Sentinel is portfolio-ready on Raspberry Pi 5 for authenticated LAN operation with local vision inference, KPI telemetry, event clips, and OpenAI-backed reasoning. Multi-cloud LLM comparison is intentionally deferred because the Gemini free-tier profile hit provider rate limits during evaluation.

## Pi 5 Stability

| Metric | Value |
|---|---:|
| 24h soak result | PASS |
| Availability | 100.0% |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Temperature max | 55.6 C |
| Memory max | 23.2% |
| RTSP reconnect delta | 0.0 |
| Throttled samples | 0 |

## Dashboard Auth Smoke

| Metric | Value |
|---|---:|
| Authenticated smoke result | UNKNOWN |
| Capture FPS avg/min | None / None |
| Inference avg/max | None / None ms |
| Temperature max | None C |

## LLM Reasoning Baseline

| Provider | Model | Success | Risk Accuracy | Avg Latency | P95 Latency |
|---|---|---:|---:|---:|---:|
| -- | -- | -- | -- | -- | -- |

## Recommendation

Use the currently validated OpenAI profile as the production reasoning provider.
Keep Gemini disabled for automated comparison until paid quota or a slower rate-limited evaluator is required.

## Next Gates

1. Capture final architecture diagram and short demo video.
2. Run one INT8 detector comparison against representative clips.
3. Prepare interview talking points around edge constraints, telemetry gates, and LLM fallback design.
