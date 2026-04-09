#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Resetting Sentinel project structure in: $ROOT_DIR"

# Remove existing files/folders that should be recreated
rm -rf \
  "$ROOT_DIR/models" \
  "$ROOT_DIR/data" \
  "$ROOT_DIR/src" \
  "$ROOT_DIR/include" \
  "$ROOT_DIR/README.md"

# Recreate directory structure
mkdir -p "$ROOT_DIR/models/yolo"
mkdir -p "$ROOT_DIR/data/videos"
mkdir -p "$ROOT_DIR/src/app"
mkdir -p "$ROOT_DIR/src/capture"
mkdir -p "$ROOT_DIR/src/detection"
mkdir -p "$ROOT_DIR/src/ui"
mkdir -p "$ROOT_DIR/src/utils"
mkdir -p "$ROOT_DIR/include"

# Recreate files
: > "$ROOT_DIR/CMakeLists.txt"
: > "$ROOT_DIR/README.md"

: > "$ROOT_DIR/models/yolo/model.onnx"
: > "$ROOT_DIR/data/videos/demo.mp4"

: > "$ROOT_DIR/src/main.cpp"

: > "$ROOT_DIR/src/app/pipeline.h"
: > "$ROOT_DIR/src/app/pipeline.cpp"

: > "$ROOT_DIR/src/capture/video_source.h"
: > "$ROOT_DIR/src/capture/video_source.cpp"

: > "$ROOT_DIR/src/detection/detector.h"
: > "$ROOT_DIR/src/detection/detector.cpp"
: > "$ROOT_DIR/src/detection/yolo_onnx.h"
: > "$ROOT_DIR/src/detection/yolo_onnx.cpp"
: > "$ROOT_DIR/src/detection/postprocess.h"
: > "$ROOT_DIR/src/detection/postprocess.cpp"

: > "$ROOT_DIR/src/ui/overlay_renderer.h"
: > "$ROOT_DIR/src/ui/overlay_renderer.cpp"

: > "$ROOT_DIR/src/utils/timer.h"
: > "$ROOT_DIR/src/utils/timer.cpp"

echo "Done."