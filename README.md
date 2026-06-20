# Sentinel

Sentinel is a real-time edge-AI person-monitoring system built with C++17,
OpenCV, ONNX Runtime, and a YOLOv8 ONNX model. On Raspberry Pi 5 it runs as a
headless `systemd` appliance with RTSP capture, local person detection, zone and
loitering events, clip recording, KPI telemetry, an authenticated operations
dashboard, and an asynchronous LLM reasoning sidecar.

The CMake configuration detects the target automatically:

- Windows builds use the `windows` variant and run inference on every frame.
- ARM Linux builds use the `raspberrypi` variant and run inference every third
  frame to reduce Raspberry Pi CPU load.

The selected variant is printed during CMake configuration and when Sentinel
starts.

## Raspberry Pi 5 Evidence

The Pi 5 4 GB deployment has a completed evidence bundle under
`Results/sentinel-reports/`:

- `pi5-24h-soak.json` and `pi5-24h-soak.md`: 24-hour stability run.
- `pi5-auth-smoke.json`: authenticated dashboard smoke test.
- `llm-comparison.json` and `llm-comparison.md`: labeled reasoning baseline.
- `sentinel-pi5-final-report.md`: final project evidence summary.
- `dashboard-final.png`: final dashboard screenshot.

For interview framing, see [`docs/portfolio.md`](docs/portfolio.md) and
[`docs/edge-ai-architect-story.md`](docs/edge-ai-architect-story.md). For a
short walkthrough recording plan, see [`docs/demo-script.md`](docs/demo-script.md).
On Raspberry Pi Desktop or VNC, `deploy/pi5/run_gui_demo.sh` runs the same
pipeline with `SENTINEL_HEADLESS=0` so the live OpenCV camera window can be
recorded for a more visual demo.

Current validated baseline:

| Metric | Result |
|---|---:|
| 24-hour availability | 100.0% |
| Capture FPS avg/min | 14.95 / 14.1 |
| Detection FPS avg | 4.98 |
| Inference avg/max | 54.4 / 59.06 ms |
| Max temperature | 55.6 C |
| RTSP reconnect delta | 0 |
| Throttling | 0 samples |
| Dashboard auth smoke | PASS |
| OpenAI reasoning baseline | 100% success, 100% risk accuracy |

Gemini free-tier comparison is intentionally deferred because provider quota
limits caused `429`/`503` responses during evaluation. The production reasoning
profile remains `openai-cloud` with `gpt-4.1-mini`; `mock` is retained as the
deterministic fallback and latency baseline.

## Requirements

- CMake 3.20 or newer
- OpenCV development files
- ONNX Runtime development package
- `models/yolo/model_320.onnx`

Set `ONNXRUNTIME_DIR` to the ONNX Runtime installation root. The directory must
contain `include/onnxruntime_cxx_api.h` and the platform library under `lib/`.

## Windows

Open PowerShell in the repository root.

```powershell
$env:ONNXRUNTIME_DIR = "D:\path\to\onnxruntime"

cmake -S . -B build
cmake --build build --config Release
```

Ensure the OpenCV and ONNX Runtime DLL directories are on `PATH` before
starting Sentinel:

```powershell
$env:PATH = "$env:ONNXRUNTIME_DIR\lib;D:\path\to\opencv\build\x64\vc16\bin;$env:PATH"
```

Run with the default webcam:

```powershell
.\build\Release\sentinel.exe
```

Run with a specific webcam index:

```powershell
.\build\Release\sentinel.exe "1"
```

Run with an RTSP stream:

```powershell
.\build\Release\sentinel.exe "rtsp://username:password@camera-ip:554/stream"
```

## Raspberry Pi

Use a 64-bit Raspberry Pi OS installation. Install OpenCV and build tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake libopencv-dev
```

Install the ARM64 ONNX Runtime package and set its location:

```bash
wget https://github.com/microsoft/onnxruntime/releases/download/v1.24.4/onnxruntime-linux-aarch64-1.24.4.tgz
tar -xzf onnxruntime-linux-aarch64-1.24.4.tgz
sudo mv onnxruntime-linux-aarch64-1.24.4 /opt/onnxruntime

export ONNXRUNTIME_DIR=/opt/onnxruntime
export LD_LIBRARY_PATH="$ONNXRUNTIME_DIR/lib:$LD_LIBRARY_PATH"
```

Build Sentinel:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Run with the default webcam:

```bash
./build/sentinel
```

Run with a specific webcam index:

```bash
./build/sentinel "1"
```

Run with an RTSP stream:

```bash
./build/sentinel "rtsp://username:password@camera-ip:554/stream"
```

For a Raspberry Pi 5 4 GB headless installation managed by `systemd`, see
[`docs/pi5-deployment.md`](docs/pi5-deployment.md).

The Pi deployment includes:

- `sentinel.service` for the C++ vision pipeline.
- `sentinel-dashboard.service` for the FastAPI KPI API and dashboard.
- `sentinel-llm.service` for asynchronous reasoning and model evaluation.

## Other Video Sources

A video-file path can be passed instead of a webcam index or RTSP URL:

```text
sentinel path/to/video.mp4
```

Press `Q` or `Esc` in the Sentinel window to exit. Loitering-triggered clips
are saved under `data/clips/`.

## Notes

- Run Sentinel from the repository root so the relative model path resolves.
- Generic desktop Linux and macOS targets are currently rejected by CMake.
- On Pi, loitering events queue asynchronous reasoning requests under
  `/var/lib/sentinel/kpi`; provider latency does not block capture or
  inference.
