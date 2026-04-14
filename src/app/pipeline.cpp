#include "app/pipeline.h"

#include "capture/video_source.h"
#include "detection/yolo_onnx.h"
#include "events/event_engine.h"
#include "reasoning/scene_state.h"
#include "tracking/tracker.h"
#include "ui/overlay_renderer.h"
#include "utils/timer.h"
#include "zones/zone_manager.h"

#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>
#include <vector>

Pipeline::Pipeline(const std::string& source)
    : source_(source) {}

bool Pipeline::initialize() {
    std::cout << "[Sentinel] Pipeline initialized.\n";
    return true;
}

void Pipeline::run() {
    std::cout << "[Sentinel] Entering run().\n";

    const std::string model_path = "models/yolo/model.onnx";
    YoloOnnxDetector detector(model_path);

    if (!detector.initialize()) {
        std::cerr << "[Sentinel] Failed to initialize detector.\n";
        return;
    }

    VideoSource video_source(source_);

    if (!video_source.open()) {
        std::cerr << "[Sentinel] Could not open video source.\n";
        return;
    }

    Tracker tracker;
    EventEngine event_engine;
    ZoneManager zone_manager;
    SceneStateBuilder scene_state_builder;
    OverlayRenderer overlay_renderer;

    cv::Mat frame;
    const std::string window_name = "Sentinel";
    const std::string source_label = (source_.empty() || source_ == "0") ? "webcam" : source_;

    double fps = 0.0;
    std::vector<std::string> recent_events;
    bool zones_initialized = false;
    double last_scene_print_time_sec = -1000.0;

    std::cout << "[Sentinel] Starting frame loop.\n";

    Timer app_timer;
    app_timer.start();

    while (true) {
        Timer frame_timer;
        frame_timer.start();

        if (!video_source.read(frame)) {
            std::cout << "[Sentinel] End of stream or failed frame read.\n";
            break;
        }

        if (!zones_initialized) {
            zone_manager.initialize(frame.cols, frame.rows);
            zones_initialized = true;
        }

        DetectionResult detection_result = detector.detect(frame);
        std::vector<Detection> tracked_detections = tracker.update(detection_result.detections);

        const double current_time_sec = app_timer.elapsedMilliseconds() / 1000.0;

        std::vector<std::string> frame_events = event_engine.update(tracked_detections, current_time_sec);
        for (const auto& event : frame_events) {
            std::cout << event << "\n";
            recent_events.push_back(event);
        }

        std::vector<ZoneEvent> zone_events = zone_manager.update(tracked_detections, current_time_sec);
        for (const auto& zone_event : zone_events) {
            std::cout << zone_event.message << "\n";
            recent_events.push_back(zone_event.message);
        }

        if (recent_events.size() > 10) {
            recent_events.erase(
                recent_events.begin(),
                recent_events.begin() + static_cast<std::ptrdiff_t>(recent_events.size() - 10));
        }

        std::vector<Detection> person_detections;
        std::vector<std::string> person_zones;
        std::vector<bool> person_loitering_flags;

        for (const auto& det : tracked_detections) {
            if (det.class_name == "person") {
                person_detections.push_back(det);
                person_zones.push_back(zone_manager.getZoneForTrack(det.track_id));
                person_loitering_flags.push_back(zone_manager.isTrackLoitering(det.track_id));
            }
        }

        SceneState scene_state = scene_state_builder.build(
            current_time_sec,
            person_detections,
            recent_events,
            person_zones,
            person_loitering_flags);

        if ((current_time_sec - last_scene_print_time_sec) >= 2.0) {
            std::cout << "[SceneState]\n" << scene_state_builder.toJson(scene_state) << "\n";
            last_scene_print_time_sec = current_time_sec;
        }

        const double frame_time_ms = frame_timer.elapsedMilliseconds();

        if (frame_time_ms > 0.0) {
            const double instant_fps = 1000.0 / frame_time_ms;
            if (fps == 0.0) {
                fps = instant_fps;
            } else {
                fps = 0.9 * fps + 0.1 * instant_fps;
            }
        }

        zone_manager.drawZones(frame);
        overlay_renderer.drawDetections(frame, person_detections);
        overlay_renderer.drawStats(frame, fps, frame_time_ms, source_label);
        overlay_renderer.drawEvents(frame, recent_events);

        cv::imshow(window_name, frame);

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "[Sentinel] Exit requested by user.\n";
            break;
        }
    }

    cv::destroyAllWindows();
    std::cout << "[Sentinel] Exiting run().\n";
}