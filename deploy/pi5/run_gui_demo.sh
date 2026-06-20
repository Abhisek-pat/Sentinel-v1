#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${DISPLAY:-}" ]]; then
  echo "DISPLAY is not set. Run this from Raspberry Pi Desktop or a VNC session."
  exit 1
fi

if systemctl is-active --quiet sentinel.service; then
  echo "Stopping sentinel.service so the demo window can use the camera stream..."
  sudo systemctl stop sentinel.service
fi

if [[ -r /etc/sentinel/sentinel.env ]]; then
  # shellcheck disable=SC1091
  source /etc/sentinel/sentinel.env
fi

SENTINEL_SOURCE="${1:-${SENTINEL_SOURCE:-}}"
if [[ -z "${SENTINEL_SOURCE}" ]]; then
  echo "No source configured. Pass a camera index, video file, or RTSP URL:"
  echo "  bash deploy/pi5/run_gui_demo.sh 'rtsp://user:pass@camera-ip:554/stream2'"
  exit 1
fi

DEMO_ROOT="${HOME}/sentinel-demo"
mkdir -p "${DEMO_ROOT}/clips" "${DEMO_ROOT}/kpi"

export SENTINEL_SOURCE
export SENTINEL_HEADLESS=0
export SENTINEL_MODEL_PATH="${SENTINEL_MODEL_PATH:-/opt/sentinel/share/sentinel/models/yolo/model_320.onnx}"
export SENTINEL_CLIP_DIR="${DEMO_ROOT}/clips"
export SENTINEL_KPI_DIR="${DEMO_ROOT}/kpi"
export SENTINEL_INFERENCE_INTERVAL="${SENTINEL_INFERENCE_INTERVAL:-3}"
export SENTINEL_CLIP_BUFFER_FRAMES="${SENTINEL_CLIP_BUFFER_FRAMES:-30}"
export SENTINEL_MAX_CLIPS="${SENTINEL_MAX_CLIPS:-20}"
export SENTINEL_TELEMETRY_INTERVAL_SEC="${SENTINEL_TELEMETRY_INTERVAL_SEC:-30}"
export SENTINEL_SCENE_INTERVAL_SEC="${SENTINEL_SCENE_INTERVAL_SEC:-30}"
export SENTINEL_REASONING_COOLDOWN_SEC="${SENTINEL_REASONING_COOLDOWN_SEC:-60}"

echo "Starting Sentinel GUI demo window."
echo "Press Q or Esc in the Sentinel window to stop."
echo "Demo clips: ${DEMO_ROOT}/clips"
echo "Demo KPI:   ${DEMO_ROOT}/kpi"

cd /opt/sentinel
/opt/sentinel/bin/sentinel
