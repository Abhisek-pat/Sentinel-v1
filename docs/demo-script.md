# Sentinel Demo Script

Target length: 90-120 seconds.

## Recording Setup

1. Open the dashboard:

```text
http://<pi-ip-address>:8080/?token=<dashboard-token>
```

2. For a more visual LinkedIn demo, run the OpenCV camera window from Raspberry
   Pi Desktop or VNC:

```bash
bash deploy/pi5/run_gui_demo.sh
```

If `/etc/sentinel/sentinel.env` is not readable by your desktop user, pass the
source explicitly:

```bash
bash deploy/pi5/run_gui_demo.sh "rtsp://user:pass@camera-ip:554/stream2"
```

Press `Q` or `Esc` in the Sentinel window to stop. Restart the background
service after recording:

```bash
sudo systemctl start sentinel
```

3. Open a terminal on the Pi with:

```bash
sudo systemctl status sentinel sentinel-dashboard sentinel-llm --no-pager
tail -f /var/lib/sentinel/kpi/reasoning_results.jsonl
```

4. Keep the final evidence report open:

```bash
less ~/sentinel-reports/sentinel-pi5-final-report.md
```

## Voiceover

### 1. Opening

“This is Sentinel, an Edge AI monitoring appliance running on Raspberry Pi 5.
It performs local person detection, zone tracking, loitering detection, event
clip recording, KPI monitoring, and asynchronous LLM reasoning.”

### 2. Dashboard

Show the dashboard top cards.

“The live pipeline is capturing at roughly 15 FPS, detecting at about 5 FPS, and
running CPU inference around 54 milliseconds. The Pi is below 60 degrees Celsius
with no throttling and no RTSP reconnect growth.”

### 3. Event Quality And Reasoning

Scroll to event quality, reasoning, and recent events.

“Loitering and zone events are persisted as structured records. When loitering
is detected, Sentinel queues a SceneState for the LLM sidecar. That keeps cloud
latency out of the real-time inference path.”

### 4. Evidence

Show the final report.

“The project was validated with a 24-hour soak test: 100 percent availability,
stable inference latency, no throttling, and no reconnect growth. Dashboard
authentication also passed an authenticated smoke test.”

### 5. Architecture Close

Show `docs/assets/sentinel-data-flow.png`.

“The key architectural choice is separation of concerns: C++ handles real-time
capture and inference, FastAPI handles observability and reasoning, and model
providers are abstracted so OpenAI, local models, or future providers can be
evaluated without changing the vision loop.”

## Shots Checklist

- Live OpenCV Sentinel camera window with boxes/events.
- Dashboard top KPI cards.
- Vision Performance and Pi Health.
- Reasoning section.
- Recent Events section.
- Final evidence report.
- Architecture diagram.

## Optional Closing Line

“This project demonstrates the full edge-AI lifecycle: model deployment,
real-time constraints, observability, reliability evidence, and LLM integration
with operational fallback behavior.”
