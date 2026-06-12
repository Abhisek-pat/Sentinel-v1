# Raspberry Pi 5 4 GB Deployment

## Strategy

The Pi should be treated as an edge vision appliance, not a development
desktop. Run one 64-bit headless Sentinel process, use the camera's low
resolution substream, perform local person detection and event recording, and
keep the optional Python/OpenAI reasoning service off the Pi.

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

The current service listens on port 8080 for LAN testing. Authentication and
secure remote access are scheduled before production exposure.

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
[Telemetry] capture_fps=15.01 detection_fps=5.00 preprocess_ms=... inference_ms=... postprocess_ms=... persons=...
```

For the configured interval of three, `detection_fps` should be approximately
one third of `capture_fps`. Investigate sustained capture-rate drops, rising
inference latency, thermal throttling, or repeated RTSP reconnects.
`last_frame_age_ms` should normally remain well below 1000.

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
