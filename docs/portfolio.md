# Sentinel Portfolio Notes

## Project Claim

Sentinel is an edge-AI monitoring appliance validated on Raspberry Pi 5 4 GB. It
runs local person detection at the edge, records loitering clips, exposes KPI
telemetry through an authenticated dashboard, and uses an asynchronous LLM
sidecar for event reasoning without blocking the real-time vision loop.

## Evidence Bundle

Use `Results/sentinel-reports/` as the final evidence folder.

| Artifact | Purpose |
|---|---|
| `sentinel-pi5-final-report.md` | One-page final evidence summary |
| `pi5-24h-soak.json` | Raw 24-hour stability report |
| `pi5-24h-soak.md` | Human-readable 24-hour soak analysis |
| `pi5-auth-smoke.json` | Authenticated dashboard smoke test |
| `llm-comparison.json` | Labeled reasoning comparison data |
| `llm-comparison.md` | Human-readable reasoning comparison |
| `diagnostics.txt` | Pi service, system, and API diagnostics |

## Validated Results

| Metric | Result |
|---|---:|
| 24-hour availability | 100.0% |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Max temperature | 55.6 C |
| RTSP reconnect delta | 0 |
| Throttled samples | 0 |
| Dashboard auth smoke | PASS |
| OpenAI reasoning baseline | 100% success, 100% risk accuracy |

## Interview Talking Points

1. Real-time edge design: frame capture and inference stay local on the Pi, with
   RTSP newest-frame delivery to avoid stale processing.
2. Reliability gate: the 24-hour soak validated availability, thermal headroom,
   no throttling, no RTSP reconnect growth, and stable inference latency.
3. Observability: KPI JSONL records feed a SQLite-backed dashboard with alerts
   for stale telemetry, low FPS, thermal issues, reconnects, reasoning backlog,
   and failure rates.
4. LLM architecture: loitering SceneState is queued asynchronously; cloud latency
   cannot block capture or inference; `mock` remains a deterministic fallback.
5. Model governance: OpenAI `gpt-4.1-mini` is the validated production reasoning
   provider, while Gemini free-tier comparison was deferred after quota-related
   `429` and `503` responses.
6. Security baseline: dashboard token authentication is enabled before exposing
   the Pi dashboard outside trusted local testing.

## Remaining Optional Enhancements

1. INT8 detector comparison against representative clips.
2. Local or LAN LLM profile once a Pi-friendly model endpoint is selected.
3. Reverse proxy or VPN for secure remote dashboard access.
4. Demo video showing dashboard, event clip, and reasoning assessment.
