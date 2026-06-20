# Edge AI Architect Story

## One-Minute Narrative

Sentinel is an edge-AI monitoring appliance that turns a Raspberry Pi 5 into a
validated real-time vision node. The system captures an RTSP substream, performs
local YOLO person detection through ONNX Runtime, tracks people, detects zone
events and loitering, records evidence clips, and publishes operational KPIs to
an authenticated dashboard. LLM reasoning is isolated in a sidecar and triggered
asynchronously, so cloud latency never blocks frame capture or inference.

The project is valuable as an Edge AI architecture case study because it covers
the full product path: model execution on constrained hardware, thermal and
memory validation, event semantics, operational monitoring, secure dashboard
access, cloud-model fallback behavior, and a 24-hour reliability gate.

## Architecture Decisions

| Decision | Why It Matters |
|---|---|
| RTSP newest-frame capture | Prevents stale frame backlog when decode or inference stalls. |
| 320x320 YOLO ONNX input | Keeps Pi 5 CPU inference around 54 ms while preserving useful person detection. |
| Inference every third frame | Maintains ~15 FPS capture and ~5 FPS detection on Pi 5 4 GB. |
| JSONL KPI/event persistence | Gives simple, debuggable, crash-tolerant telemetry ingestion. |
| FastAPI dashboard sidecar | Keeps observability separate from the C++ real-time loop. |
| Async LLM sidecar | Allows OpenAI reasoning without blocking edge inference. |
| `mock` fallback provider | Gives deterministic behavior when a cloud model fails or quota is exhausted. |
| 24-hour soak gate | Converts “it works” into measurable operational evidence. |

## Validated Outcomes

| Metric | Result |
|---|---:|
| 24-hour availability | 100.0% |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Temperature max | 55.6 C |
| RTSP reconnect delta | 0 |
| Throttled samples | 0 |
| Dashboard auth smoke | PASS |
| OpenAI reasoning baseline | 100% success, 100% risk accuracy |

## Project Positioning

Use this project to show that you can reason across the whole edge stack:

1. Hardware constraints: Pi 5 4 GB, CPU-only inference, thermal headroom, memory headroom.
2. Video reliability: RTSP recovery, frame freshness, capture delivery efficiency.
3. AI runtime choices: ONNX Runtime, model input sizing, inference interval, future INT8 path.
4. Event intelligence: tracking, zones, loitering, clips, SceneState.
5. Observability: telemetry, alerts, dashboard, diagnostic scripts, final evidence reports.
6. LLM governance: provider abstraction, fallback, latency isolation, free-tier quota handling.
7. Security baseline: dashboard token auth before moving beyond trusted LAN access.

## How To Explain Gemini

Gemini was configured successfully, but free-tier evaluation hit provider-side
`429` and `503` responses. The architecture handled that cleanly: Gemini was
deferred, OpenAI remained the validated production provider, and `mock` remained
the deterministic fallback. This is a model-governance decision, not a failure
of the edge vision pipeline.

## Next Technical Extension

The next measurable upgrade is INT8 detector validation. The acceptance rule
should be conservative: adopt INT8 only if it improves inference latency without
meaningfully reducing person/event reliability on representative footage.
