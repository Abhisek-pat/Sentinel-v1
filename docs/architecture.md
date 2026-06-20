# Sentinel Architecture and Data Flow

Sentinel is a real-time person-monitoring pipeline that ingests webcam,
video-file, or RTSP frames; performs person-only YOLOv8 inference; tracks
people; evaluates scene and zone events; records loitering clips; persists KPI
telemetry; exposes an authenticated operations dashboard; and optionally
enriches loitering events through an asynchronous LLM reasoning sidecar.

## End-to-End Data Flow

![Sentinel end-to-end data flow](assets/sentinel-data-flow.png)

The diagram uses the **Midnight Operations Dashboard** theme:

- Cyan paths carry frames and display output.
- Green paths represent detection and tracking work.
- Amber paths represent event and zone evaluation.
- Red panels represent recording responses.
- Dashed purple paths are sidecar components that run outside the real-time
  inference loop. On the Pi deployment, the reasoning sidecar is connected
  asynchronously through JSONL queue files so provider latency cannot block
  capture or inference.

## Runtime Flow

1. `main.cpp` chooses the video source and starts `Pipeline`.
2. `VideoSource` opens the webcam, file, or RTSP stream. RTSP capture runs in a background thread and exposes only the newest frame.
3. Every frame enters the main loop and the configured clip ring buffer.
4. Every third frame is letterboxed to 320 by 320 and passed through the YOLOv8 ONNX model.
5. Post-processing keeps person detections, maps boxes to the original frame, and applies non-maximum suppression.
6. The IoU tracker assigns persistent IDs.
7. `EventEngine` calculates dwell time and emits scene entry and exit events.
8. `ZoneManager` detects zone entry, exit, and loitering. Loitering triggers an event clip save.
9. `SceneStateBuilder` creates structured JSON from people, zones, dwell times, flags, and recent events.
10. Telemetry, events, reasoning requests, and clip records are persisted under `/var/lib/sentinel/kpi` on Pi.
11. The dashboard sidecar ingests JSONL records into SQLite and serves KPI, event, reasoning, and evaluation APIs.
12. The reasoning sidecar consumes loitering-triggered SceneState requests asynchronously and writes model results back for dashboard ingestion.

## Reasoning Service

`llm_service/` is a provider-neutral FastAPI sidecar. It currently ships with a
deterministic `mock` provider that validates the service contract without API
keys or network access. The service exposes:

- `POST /reason` for one provider inference.
- `POST /benchmark` for repeated, comparable provider runs.
- `GET /evaluation/cases` for the reusable labeled SceneState evaluation set.
- `POST /evaluate` for provider correctness and latency comparison.
- `GET /providers` for provider and model discovery.
- `GET /health` for service readiness.

Each successful response includes the summary, risk level, recommended action,
provider, model, and request latency. OpenAI-compatible model profiles can be
configured without changing application code. The benchmark endpoint returns
individual runs plus per-model success rate, average/minimum/maximum latency,
and risk-level counts.

The evaluation endpoint runs every selected provider against labeled low,
medium, and high-risk SceneStates. It reports provider availability, risk
classification accuracy, and latency. Evaluation is always explicitly invoked
and is never part of the live event-processing path.

Compact per-provider evaluation summaries are appended to
`evaluation_results.jsonl`, ingested into the dashboard SQLite database, and
shown in the Model Evaluation section. Full per-case output remains in the
explicit `/evaluate` response.

The balanced evaluation suite contains fifteen labeled cases: five each for
low, medium, and high risk. Comparisons include overall and per-risk accuracy,
a risk confusion matrix, success rate, and average/minimum/maximum/p95 latency.

Sentinel asynchronously queues a SceneState when loitering is detected. Queue
writes occur inside the vision process, but provider requests run in a
background worker owned by the reasoning sidecar. The worker persists its
queue offset, falls back to `mock` when the selected provider fails, and writes
results for dashboard ingestion. Provider latency cannot block capture or
inference.

The dashboard summarizes reasoning request count, success rate, fallback use,
latency, provider/model usage, and pending queue depth. It alerts when requests
are failing repeatedly or the asynchronous queue is backing up.

`src/reasoning/llm_client.*` is the earlier Windows-only synchronous prototype
and remains outside the CMake target. The production Pi path uses the JSONL
queue plus `sentinel-llm.service`.

## Validated Pi 5 Baseline

The current evidence bundle is stored under `Results/sentinel-reports/`.

| Gate | Result |
|---|---:|
| 24-hour soak | PASS |
| Availability | 100.0% |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Temperature max | 55.6 C |
| RTSP reconnect delta | 0 |
| Throttling | 0 samples |
| Dashboard auth smoke | PASS |
| OpenAI reasoning baseline | 100% success, 100% risk accuracy |

## Key Outputs

- Live OpenCV monitoring window on desktop builds
- Headless Pi vision service logs and persisted KPI JSONL records
- Authenticated dashboard API and browser UI
- Scene-state JSON at the configured interval
- Loitering-triggered clips under `/var/lib/sentinel/clips` on Pi
- Asynchronous reasoning results and labeled model-evaluation reports
