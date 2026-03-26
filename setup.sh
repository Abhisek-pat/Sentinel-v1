#!/bin/bash

# Ensure script is run inside "Sentinel-v1"
if [ "$(basename "$PWD")" != "Sentinel-v1" ]; then
  echo "❌ Please run this script inside the 'Sentinel-v1' folder"
  exit 1
fi

echo "Creating project structure inside $(pwd)..."

# Create directories
mkdir -p configs
mkdir -p models/yolo
mkdir -p src/app
mkdir -p src/capture
mkdir -p src/detection
mkdir -p src/tracking
mkdir -p src/events
mkdir -p src/reasoning
mkdir -p src/storage
mkdir -p src/ui
mkdir -p llm_service/prompt_templates
mkdir -p data/videos
mkdir -p data/outputs
mkdir -p docs

# Create files
touch CMakeLists.txt
touch README.md

touch configs/default.yaml
touch configs/zones.json
touch configs/classes.json

touch src/main.cpp
touch src/app/pipeline.cpp
touch src/capture/video_source.cpp
touch src/detection/detector.cpp
touch src/detection/postprocess.cpp
touch src/tracking/tracker.cpp
touch src/events/event_engine.cpp
touch src/events/zone_logic.cpp
touch src/reasoning/scene_state.cpp
touch src/reasoning/llm_client.cpp
touch src/storage/event_logger.cpp
touch src/storage/metrics_logger.cpp
touch src/ui/overlay_renderer.cpp

touch llm_service/app.py

touch docs/architecture.md
touch docs/performance.md

echo "✅ Sentinel project structure created successfully!"