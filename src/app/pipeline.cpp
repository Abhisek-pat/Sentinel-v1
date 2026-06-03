#include "app/pipeline.h"

#include "capture/video_source.h"
#include "detection/yolo_onnx.h"
#include "events/event_engine.h"
#include "reasoning/scene_state.h"
#include "recording/frame_ring_buffer.h"
#include "tracking/tracker.h"
#include "ui/overlay_renderer.h"
#include "utils/timer.h"
#include "zones/zone_manager.h"

#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace {

void ensureDirectoryExists(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

bool isImportantForUi(const std::string& event) {
    return event.find("loitering") != std::string::npos ||
           event.find("exited") != std::string::npos ||
           event.find("entered scene") != std::string::npos;
}

}  // namespace

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
    const std::string source_label = "";

    std::vector<std::string> recent_events;
    bool zones_initialized = false;

    double last_scene_print_time_sec = -1000.0;
    double last_clip_save_time_sec = -1000.0;

    std::string llm_summary = "Pi vision mode: LLM disabled.";
    std::string llm_risk = "low";

    std::vector<Detection> tracked_detections;
    std::vector<Detection> person_detections;

    int frame_count = 0;

    double pipeline_fps = 0.0;
    double displayed_frame_time_ms = 0.0;

    ensureDirectoryExists("data");
    ensureDirectoryExists("data/clips");

    FrameRingBuffer clip_buffer(60);
    int clip_id = 0;

    std::cout << "[Sentinel] Starting frame loop.\n";

    Timer app_timer;
    app_timer.start();

    while (true) {
        if (!video_source.read(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        clip_buffer.push(frame);

        if (!zones_initialized) {
            zone_manager.initialize(frame.cols, frame.rows);
            zones_initialized = true;
        }

        frame_count++;

        // For Pi, keep this at every 2nd frame first.
        // If Pi struggles, change 2 to 3 or 4.
        const bool run_inference_this_frame = (frame_count % 2 == 0);

        if (run_inference_this_frame) {
            Timer processing_timer;
            processing_timer.start();

            bool should_save_event_clip = false;
            bool loitering_detected = false;

            DetectionResult detection_result = detector.detect(frame);
            tracked_detections = tracker.update(detection_result.detections);

            const double current_time_sec = app_timer.elapsedMilliseconds() / 1000.0;

            std::vector<std::string> frame_events =
                event_engine.update(tracked_detections, current_time_sec);

            for (const auto& event : frame_events) {
                std::cout << event << "\n";

                if (isImportantForUi(event)) {
                    recent_events.push_back(event);
                }
            }

            std::vector<ZoneEvent> zone_events =
                zone_manager.update(tracked_detections, current_time_sec);

            for (const auto& zone_event : zone_events) {
                std::cout << zone_event.message << "\n";

                if (zone_event.message.find("loitering") != std::string::npos) {
                    should_save_event_clip = true;
                    loitering_detected = true;
                    recent_events.push_back(zone_event.message);
                } else if (zone_event.message.find("exited") != std::string::npos) {
                    recent_events.push_back(zone_event.message);
                }
            }

            if (should_save_event_clip &&
                (current_time_sec - last_clip_save_time_sec) >= 10.0) {
                std::ostringstream clip_path;
                clip_path << "data/clips/event_"
                          << std::setw(4) << std::setfill('0') << clip_id++
                          << "_t" << static_cast<int>(current_time_sec)
                          << ".avi";

                if (clip_buffer.saveToVideo(clip_path.str(), 10.0)) {
                    std::cout << "[Sentinel] Event clip saved: "
                              << clip_path.str() << "\n";
                }

                last_clip_save_time_sec = current_time_sec;
            }

            if (recent_events.size() > 10) {
                recent_events.erase(
                    recent_events.begin(),
                    recent_events.begin() + static_cast<std::ptrdiff_t>(recent_events.size() - 10));
            }

            person_detections.clear();

            std::vector<std::string> person_zones;
            std::vector<bool> person_loitering_flags;

            for (const auto& det : tracked_detections) {
                if (det.class_name == "person") {
                    person_detections.push_back(det);
                    person_zones.push_back(zone_manager.getZoneForTrack(det.track_id));
                    person_loitering_flags.push_back(zone_manager.isTrackLoitering(det.track_id));
                }
            }

            if (person_detections.empty()) {
                llm_summary = "No active person detected.";
                llm_risk = "low";
            } else if (loitering_detected) {
                llm_summary = "Loitering detected in monitored zone.";
                llm_risk = "medium";
            } else {
                llm_summary = "Person detected. Monitoring active.";
                llm_risk = "low";
            }

            SceneState scene_state = scene_state_builder.build(
                current_time_sec,
                person_detections,
                recent_events,
                person_zones,
                person_loitering_flags);

            const std::string scene_json = scene_state_builder.toJson(scene_state);

            if ((current_time_sec - last_scene_print_time_sec) >= 5.0) {
                std::cout << "[SceneState]\n" << scene_json << "\n";
                last_scene_print_time_sec = current_time_sec;
            }

            const double processing_time_ms = processing_timer.elapsedMilliseconds();
            displayed_frame_time_ms = processing_time_ms;

            if (processing_time_ms > 0.0) {
                const double instant_pipeline_fps = 1000.0 / processing_time_ms;
                if (pipeline_fps == 0.0) {
                    pipeline_fps = instant_pipeline_fps;
                } else {
                    pipeline_fps = 0.9 * pipeline_fps + 0.1 * instant_pipeline_fps;
                }
            }
        }

        zone_manager.drawZones(frame);
        overlay_renderer.drawDetections(frame, person_detections);
        overlay_renderer.drawStats(frame, pipeline_fps, displayed_frame_time_ms, source_label);
        overlay_renderer.drawLlmOutput(frame, llm_summary, llm_risk);
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