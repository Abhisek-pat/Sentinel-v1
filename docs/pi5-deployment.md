# Raspberry Pi 5 4 GB Deployment

## Strategy

The Pi should be treated as an edge vision appliance, not a development
desktop. Run one 64-bit headless Sentinel process, use the camera's low
resolution substream, perform local person detection and event recording, and
keep the optional Python/OpenAI reasoning service isolated from the real-time
vision process.

Recommended baseline:

- Raspberry Pi OS Lite 64-bit on a good A2 microSD card or USB SSD.
- Official 27 W USB-C power supply and active cooling.
- 1280x720, 10-15 FPS H.264 RTSP substream or a 720p USB camera.
- YOLOv8n FP32 at 320x320 through ONNX Runtime CPU.
- Two ONNX Runtime inference threads and inference every third frame.
- Headless operation under `systemd`.
- Thirty-frame event buffer, with clips stored under `/var/lib/sentinel/clips`.
- A maximum of 100 retained event clips to prevent unbounded disk usage.
- Performance telemetry written to the service journal every 30 seconds.
- SceneState JSON limited to once every 30 seconds.

This profile leaves substantial room inside 4 GB. The largest controllable
memory cost is the uncompressed clip ring buffer: 30 BGR frames at 1280x720
need about 83 MiB, and saving temporarily clones that buffer. A 1080p stream
more than doubles that cost and also increases decode, drawing, and recording
work without improving the 320x320 detector input.

## Install

Copy or clone the repository onto a Pi running 64-bit Raspberry Pi OS, then:

```bash
cd Sentinel-v1
sudo bash deploy/pi5/install.sh
sudo nano /etc/sentinel/sentinel.env
sudo systemctl start sentinel
sudo systemctl status sentinel
```

For RTSP, set `SENTINEL_SOURCE` to the camera's low-resolution substream URL.
The environment file is readable only by root and the `sentinel` group.

Useful operations:

```bash
sudo journalctl -u sentinel -f
sudo systemctl restart sentinel
sudo systemctl stop sentinel
sudo bash deploy/pi5/diagnose.sh
```

## KPI API

The Pi installer deploys a lightweight FastAPI sidecar that ingests Sentinel's
structured telemetry and event records into SQLite.

```bash
sudo systemctl start sentinel-dashboard
sudo systemctl status sentinel-dashboard
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/kpis/latest
curl http://127.0.0.1:8080/api/summary
curl "http://127.0.0.1:8080/api/events?limit=20"
```

Open the visual operations dashboard at `http://<pi-ip-address>:8080/`.
It shows stream performance, Pi health, reconnects, event quality, and recent
events. Dashboard filters support one hour through seven days, and the SQLite
KPI database retains seven days by default. Raw KPI JSONL files are compacted
after reaching 25 MiB. Active alerts cover stale streams,
low capture FPS, slow inference, overheating, throttling, low disk space, and
excessive short zone exits. The current service listens on port 8080 for LAN testing.

Dashboard authentication is disabled until `SENTINEL_DASHBOARD_TOKEN` is set in
`/etc/sentinel/dashboard.env`. Enable it before exposing the dashboard beyond a
trusted LAN:

```bash
sudo nano /etc/sentinel/dashboard.env
sudo systemctl restart sentinel-dashboard
```

Example value:

```text
SENTINEL_DASHBOARD_TOKEN=replace-with-a-long-random-token
```

Scripts can use a bearer token:

```bash
curl -H "Authorization: Bearer replace-with-a-long-random-token" \
  http://127.0.0.1:8080/api/dashboard
```

For browser access, open the dashboard once with the token query parameter to
set the same-origin cookie:

```text
http://<pi-ip-address>:8080/?token=replace-with-a-long-random-token
```

After that, use the normal dashboard URL. Keep the token private and rotate it
if it is shared accidentally. For internet access, place the dashboard behind a
VPN or authenticated reverse proxy rather than exposing port 8080 directly.

## Reasoning API

The Pi installer deploys a separate reasoning sidecar on loopback port `8090`.
It defaults to the deterministic `mock` provider, requires no API key, and does
not affect the real-time vision process.

```bash
sudo systemctl start sentinel-llm
sudo systemctl status sentinel-llm
curl http://127.0.0.1:8090/health
curl http://127.0.0.1:8090/providers
curl -X POST http://127.0.0.1:8090/reason \
  -H "Content-Type: application/json" \
  -d '{"scene_state":{"persons":[],"recent_events":[]}}'
```

Real model profiles are configured in `/etc/sentinel/llm.env`. Each profile
targets an OpenAI-compatible `/v1/chat/completions` endpoint and can reference a
separate API-key environment variable.

```bash
sudo nano /etc/sentinel/llm.env
sudo systemctl restart sentinel-llm
curl http://127.0.0.1:8090/providers
```

Example profile value, kept on one line:

```text
SENTINEL_LLM_PROFILES_JSON='[{"name":"model-a","type":"openai_compatible","base_url":"http://192.168.1.20:11434/v1","model":"model-a","timeout_sec":60}]'
```

Replace the example address and model name with a running model endpoint.
Cloud profiles can also set `retry_count` and `retry_backoff_sec` to smooth out
short-lived provider errors such as HTTP 503 or rate-limit retries.
`/providers` validates configuration and reports a readiness detail, but it
does not contact the model server. Verify reachability with `/reason`:

```bash
curl -X POST http://127.0.0.1:8090/reason \
  -H "Content-Type: application/json" \
  -d '{"provider":"model-a","scene_state":{"persons":[],"recent_events":[]}}'
```

Use `/benchmark` to run the same SceneState repeatedly against selected model
profiles. Its comparison output includes success rate, latency statistics, and
risk-level counts.

Use the labeled evaluation suite to compare classification accuracy and
latency across models:

```bash
curl http://127.0.0.1:8090/evaluation/cases
curl -X POST http://127.0.0.1:8090/evaluate \
  -H "Content-Type: application/json" \
  -d '{"providers":["mock"],"iterations":1}'
```

Run cloud models only when intentionally benchmarking them. An evaluation
creates `case_count * provider_count * iterations` requests. For example, the
included fifteen-case suite with `mock` and `openai-cloud` creates fifteen paid
OpenAI requests:

```bash
curl -X POST http://127.0.0.1:8090/evaluate \
  -H "Content-Type: application/json" \
  -d '{"providers":["mock","openai-cloud"],"iterations":1,"label":"pi5-baseline"}'
```

Evaluation summaries persist in the dashboard and can be queried with:

```bash
curl "http://127.0.0.1:8080/api/evaluations?limit=20"
```

To avoid hand-editing one-line JSON, render a multi-provider config with:

```bash
python3 deploy/pi5/configure_llm_profiles.py \
  --profile "name=openai-fast,base_url=https://api.openai.com/v1,model=gpt-4.1-mini,api_key_env=OPENAI_API_KEY,timeout_sec=60" \
  --profile "name=gemini-flash,base_url=https://generativelanguage.googleapis.com/v1beta/openai,model=gemini-3.5-flash,api_key_env=GEMINI_API_KEY,timeout_sec=60,retry_count=3,retry_backoff_sec=2" \
  --profile "name=cloud-alt,base_url=https://YOUR_PROVIDER_BASE_URL/v1,model=REPLACE_WITH_MODEL_ID,api_key_env=CLOUD_ALT_API_KEY,timeout_sec=60" \
  --default-provider openai-fast \
  --output ~/sentinel-reports/llm.env.generated
```

Review the generated file, then install it:

```bash
sudo install -m 0640 -o root -g sentinel \
  ~/sentinel-reports/llm.env.generated /etc/sentinel/llm.env
sudo systemctl restart sentinel-llm
curl http://127.0.0.1:8090/providers
```

Use model IDs that are available in your provider account. For OpenAI, confirm
the current model list in the official API dashboard or models documentation.

Generate a ranked comparison report from the persisted dashboard results:

```bash
mkdir -p ~/sentinel-reports
curl "http://127.0.0.1:8080/api/evaluations?limit=20" \
  -o ~/sentinel-reports/evaluations.json
python3 deploy/pi5/analyze_evaluations.py \
  ~/sentinel-reports/evaluations.json \
  --label pi5-baseline \
  --json-output ~/sentinel-reports/llm-comparison.json \
  --markdown-output ~/sentinel-reports/llm-comparison.md
```

The report ranks providers by risk accuracy, success rate, p95 latency, and
average latency. Re-run the same command after adding each new model profile.

Loitering events automatically queue an asynchronous reasoning request. The
selected provider is called by the reasoning sidecar, with `mock` used as a
fallback. Sentinel queues no more than one request per
`SENTINEL_REASONING_COOLDOWN_SEC` window, which defaults to 60 seconds.

```bash
sudo journalctl -u sentinel -f
sudo journalctl -u sentinel-llm -f
tail -f /var/lib/sentinel/kpi/reasoning_results.jsonl
curl "http://127.0.0.1:8080/api/reasoning?limit=10"
```

Reasoning results appear in the operations dashboard without blocking the
vision pipeline.

## First-Device Benchmark

Run the real camera feed for at least 30 minutes. Watch logs, temperature,
throttling, memory, CPU usage, detection stability, and clip output.

```bash
htop
watch -n 2 vcgencmd measure_temp
watch -n 2 vcgencmd get_throttled
```

Telemetry lines provide the primary baseline:

```text
[Telemetry] source_fps=15.01 capture_fps=15.01 capture_delivery_percent=99.9 detection_fps=5.00 inference_ms=... capture_read_avg_ms=... capture_read_max_ms=... capture_slow_read_percent=...
```

For the configured interval of three, `detection_fps` should be approximately
one third of `capture_fps`. Investigate sustained capture-rate drops, rising
inference latency, thermal throttling, or repeated RTSP reconnects.
`last_frame_age_ms` should normally remain well below 1000.

Use source FPS and the camera-read metrics to classify sustained capture-rate
drops:

- Low `source_fps` or `capture_slow_reads > 0` indicates that camera or network
  reads are limiting throughput. Check Wi-Fi signal, camera load, RTSP
  substream FPS, and wired Ethernet before changing inference settings.
- Healthy `source_fps` with low `capture_fps` indicates that frames are arriving
  normally but are not being delivered or processed quickly enough. Inspect
  CPU usage, clip writes, and inference interval.
- `capture_delivery_percent` should normally remain above 70%. It compares
  frames acquired from the camera with fresh frames delivered to the processing
  loop. Sentinel intentionally overwrites stale RTSP frames, so values below
  100% are expected when frame age remains low.
- `capture_slow_read_percent` is more useful than the raw slow-read count when
  comparing telemetry windows of different lengths.
- `capture_read_max_ms` reveals intermittent stalls even when average read
  latency appears healthy.

Tune only one variable at a time:

1. Start with `SENTINEL_INFERENCE_INTERVAL=3`.
2. Try `2` if thermals are stable and more responsive detection is needed.
3. Try `4` if sustained load or temperature is too high.
4. Reduce the camera stream to 640x360 or 10 FPS before reducing model input.
5. Reduce `SENTINEL_CLIP_BUFFER_FRAMES` if memory or clip-write latency matters.
6. Adjust `SENTINEL_MAX_CLIPS` to match available storage and retention needs.

Avoid swap-dependent operation. If Sentinel approaches the 4 GB limit, reduce
stream resolution and clip buffering instead of increasing swap.

## Next Optimization Gate

The FP32 model is the compatibility-first baseline. After collecting Pi
latency and detection-quality measurements, test an INT8 quantized model.
Adopt it only if person detection remains reliable on representative footage.

## Portfolio Completion Checklist

Keep the final project scope limited to these remaining gates:

1. Deploy the fifteen-case evaluation suite and record a labeled Pi baseline.
2. Configure two additional model profiles and run one comparison iteration.
3. Run a 24-hour Pi soak test and retain the KPI/evaluation outputs.
4. Compare the current FP32 detector with one INT8 model.
5. Add dashboard authentication before exposing it outside the trusted LAN.
6. Capture the final architecture diagram, benchmark table, and demo video.

The project is portfolio-ready when all six gates have evidence. Additional
features should be deferred until after interview preparation begins.

Run the automated soak test from the repository:

```bash
mkdir -p ~/sentinel-reports
python3 deploy/pi5/soak_test.py \
  --duration-hours 24 \
  --interval-sec 60 \
  --output ~/sentinel-reports/pi5-24h-soak.json
```

If dashboard authentication is enabled, either export the token before running
the soak test or pass `--dashboard-token`:

```bash
export SENTINEL_DASHBOARD_TOKEN=replace-with-a-long-random-token
python3 deploy/pi5/soak_test.py \
  --duration-hours 24 \
  --interval-sec 60 \
  --output ~/sentinel-reports/pi5-24h-soak.json
```

For a quick deployment check before the full run:

```bash
python3 deploy/pi5/soak_test.py --samples 5 --interval-sec 2
```

The report records API availability, capture and inference performance,
temperature, throttling, reconnect growth, reasoning backlog, alerts, memory,
and storage. It exits successfully only when every acceptance gate passes.
Reasoning backlog is treated as a failure only when it is sustained across
multiple samples or appears in more than one percent of samples. A single
short-lived backlog spike is retained in the report as a warning signal.

Generate a comprehensive diagnostic capture at any time with:

```bash
sudo bash deploy/pi5/diagnose.sh | tee ~/sentinel-reports/diagnostics.txt
```

Analyze an existing soak report and generate a markdown summary:

```bash
python3 deploy/pi5/analyze_soak.py \
  ~/sentinel-reports/pi5-24h-soak.json \
  --markdown-output ~/sentinel-reports/pi5-24h-soak.md
```
