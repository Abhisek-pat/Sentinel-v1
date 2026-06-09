# Sentinel Architecture and Data Flow

Sentinel is a real-time person-monitoring pipeline that ingests webcam, video-file, or RTSP frames; performs person-only YOLOv8 inference; tracks people; evaluates scene and zone events; records loitering clips; and renders an operator-facing OpenCV display.

## End-to-End Data Flow

![Sentinel end-to-end data flow](assets/sentinel-data-flow.png)

The diagram uses the **Midnight Operations Dashboard** theme:

- Cyan paths carry frames and display output.
- Green paths represent detection and tracking work.
- Amber paths represent event and zone evaluation.
- Red panels represent recording responses.
- Dashed purple paths are optional components that are present in the repository but are not connected to the current runtime.

## Runtime Flow

1. `main.cpp` chooses the video source and starts `Pipeline`.
2. `VideoSource` opens the webcam, file, or RTSP stream. RTSP capture runs in a background thread and exposes only the newest frame.
3. Every frame enters the main loop and the 60-frame ring buffer.
4. Every third frame is letterboxed to 320 by 320 and passed through the YOLOv8 ONNX model.
5. Post-processing keeps person detections, maps boxes to the original frame, and applies non-maximum suppression.
6. The IoU tracker assigns persistent IDs.
7. `EventEngine` calculates dwell time and emits scene entry and exit events.
8. `ZoneManager` detects zone entry, exit, and loitering. Loitering triggers an event clip save.
9. `SceneStateBuilder` creates structured JSON from people, zones, dwell times, flags, and recent events.
10. The active pipeline generates a local risk summary and renders the final overlay.

## Optional Reasoning Service

`llm_service/app.py` and `src/reasoning/llm_client.*` define an OpenAI-backed reasoning path. This path is shown as dashed because the client is not included in the current CMake target and the pipeline does not call it.

## Key Outputs

- Live OpenCV monitoring window
- Console event messages
- Scene-state JSON printed every five seconds
- Loitering-triggered clips under `data/clips/`
