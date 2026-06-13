#include "app/pipeline.h"

#include "capture/video_source.h"
#include "detection/yolo_onnx.h"
#include "events/event_engine.h"
#include "monitoring/kpi_writer.h"
#include "reasoning/scene_state.h"
#include "recording/frame_ring_buffer.h"
#include "tracking/tracker.h"
#include "ui/overlay_renderer.h"
#include "utils/timer.h"
#include "zones/zone_manager.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string environmentString(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

int environmentInt(const char* name, int fallback, int minimum) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    try {
        return std::max(minimum, std::stoi(value));
    } catch (const std::exception&) {
        std::cerr << "[Sentinel] Ignoring invalid " << name << ": " << value << "\n";
        return fallback;
    }
}

bool environmentFlag(const char* name, bool fallback) {
    const std::string value = environmentString(name, fallback ? "1" : "0");
    return value == "1" || value == "true" || value == "TRUE" ||
           value == "yes" || value == "YES";
}

bool isImportantForUi(const std::string& event) {
    return event.find("loitering") != std::string::npos ||
           event.find("exited") != std::string::npos ||
           event.find("entered scene") != std::string::npos;
}

void enforceClipRetention(const std::filesystem::path& clip_dir, int max_clips) {
    if (max_clips <= 0) {
        return;
    }

    std::error_code error;
    std::vector<std::filesystem::directory_entry> clips;

    for (std::filesystem::directory_iterator it(clip_dir, error), end;
         !error && it != end;
         it.increment(error)) {
        if (it->is_regular_file(error) && it->path().extension() == ".avi") {
            clips.push_back(*it);
        }
    }

    if (error) {
        std::cerr << "[Sentinel] Could not inspect clip directory: "
                  << error.message() << "\n";
        return;
    }

    std::sort(
        clips.begin(),
        clips.end(),
        [](const auto& left, const auto& right) {
            std::error_code left_error;
            std::error_code right_error;
            const auto left_time = left.last_write_time(left_error);
            const auto right_time = right.last_write_time(right_error);
            if (left_error || right_error) {
                return left.path().string() < right.path().string();
            }
            return left_time < right_time;
        });

    const std::size_t keep_count = static_cast<std::size_t>(max_clips);
    const std::size_t remove_count =
        clips.size() > keep_count ? clips.size() - keep_count : 0;

    for (std::size_t i = 0; i < remove_count; ++i) {
        std::filesystem::remove(clips[i].path(), error);
        if (error) {
            std::cerr << "[Sentinel] Could not remove old clip "
                      << clips[i].path().string() << ": " << error.message() << "\n";
            error.clear();
        } else {
            std::cout << "[Sentinel] Removed old clip: "
                      << clips[i].path().string() << "\n";
        }
    }
}

}  // namespace

Pipeline::Pipeline(const std::string& source)
    : source_(source) {}

bool Pipeline::initialize() {
    std::cout << "[Sentinel] Pipeline initialized.\n";
    return true;
}

bool Pipeline::run() {
    std::cout << "[Sentinel] Entering run().\n";

    VideoSource video_source(source_);

    if (!video_source.open()) {
        std::cerr << "[Sentinel] Could not open video source.\n";
        return false;
    }

    const std::string model_path =
        environmentString("SENTINEL_MODEL_PATH", "models/yolo/model_320.onnx");
    const std::string clip_dir =
        environmentString("SENTINEL_CLIP_DIR", "data/clips");
    const std::string kpi_dir =
        environmentString("SENTINEL_KPI_DIR", "");
    const bool headless = environmentFlag("SENTINEL_HEADLESS", false);
    const int inference_interval =
        environmentInt("SENTINEL_INFERENCE_INTERVAL", SENTINEL_INFERENCE_INTERVAL, 1);
    const int clip_buffer_frames =
        environmentInt("SENTINEL_CLIP_BUFFER_FRAMES", 60, 0);
    const int max_clips =
        environmentInt("SENTINEL_MAX_CLIPS", 100, 0);
    const int telemetry_interval_sec =
        environmentInt("SENTINEL_TELEMETRY_INTERVAL_SEC", 30, 1);
    const int scene_interval_sec =
        environmentInt("SENTINEL_SCENE_INTERVAL_SEC", 5, 1);
    const int reasoning_cooldown_sec =
        environmentInt("SENTINEL_REASONING_COOLDOWN_SEC", 60, 1);

    std::cout << "[Sentinel] Headless mode: " << (headless ? "enabled" : "disabled") << "\n";
    std::cout << "[Sentinel] Inference interval: every " << inference_interval << " frame(s)\n";
    std::cout << "[Sentinel] Clip buffer: " << clip_buffer_frames << " frame(s)\n";
    std::cout << "[Sentinel] Clip retention: "
              << (max_clips > 0 ? std::to_string(max_clips) : "unlimited") << "\n";
    std::cout << "[Sentinel] Telemetry interval: " << telemetry_interval_sec << " second(s)\n";
    std::cout << "[Sentinel] SceneState interval: " << scene_interval_sec << " second(s)\n";
    std::cout << "[Sentinel] Reasoning cooldown: " << reasoning_cooldown_sec << " second(s)\n";
    std::cout << "[Sentinel] KPI persistence: "
              << (kpi_dir.empty() ? "disabled" : kpi_dir) << "\n";

    YoloOnnxDetector detector(model_path);

    if (!detector.initialize()) {
        std::cerr << "[Sentinel] Failed to initialize detector.\n";
        return false;
    }

    Tracker tracker;
    EventEngine event_engine;
    ZoneManager zone_manager;
    SceneStateBuilder scene_state_builder;
    OverlayRenderer overlay_renderer;
    KpiWriter kpi_writer(kpi_dir);

    cv::Mat frame;
    const std::string window_name = "Sentinel";
    const std::string source_label = "";

    std::vector<std::string> recent_events;
    bool zones_initialized = false;

    double last_scene_print_time_sec = -1000.0;
    double last_clip_save_time_sec = -1000.0;
    double last_reasoning_request_time_sec = -1000.0;

    std::string llm_summary = "Local vision mode: LLM disabled.";
    std::string llm_risk = "low";

    std::vector<Detection> tracked_detections;
    std::vector<Detection> person_detections;

    int frame_count = 0;

    double pipeline_fps = 0.0;
    double displayed_frame_time_ms = 0.0;

    std::error_code directory_error;
    std::filesystem::create_directories(clip_dir, directory_error);
    if (directory_error) {
        std::cerr << "[Sentinel] Could not create clip directory " << clip_dir
                  << ": " << directory_error.message() << "\n";
    }
    enforceClipRetention(clip_dir, max_clips);

    FrameRingBuffer clip_buffer(static_cast<std::size_t>(clip_buffer_frames));
    int clip_id = 0;

    std::cout << "[Sentinel] Starting frame loop.\n";

    Timer app_timer;
    app_timer.start();

    double telemetry_window_start_sec = 0.0;
    std::size_t telemetry_frames = 0;
    std::size_t telemetry_inferences = 0;
    double telemetry_preprocess_ms = 0.0;
    double telemetry_inference_ms = 0.0;
    double telemetry_postprocess_ms = 0.0;

    while (true) {
        if (!video_source.read(frame)) {
            if (video_source.hasEnded()) {
                std::cout << "[Sentinel] End of video file reached.\n";
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        clip_buffer.push(frame);

        if (!zones_initialized) {
            zone_manager.initialize(frame.cols, frame.rows);
            zones_initialized = true;
        }

        frame_count++;
        telemetry_frames++;

        const bool run_inference_this_frame =
            (frame_count % inference_interval == 0);

        if (run_inference_this_frame) {
            Timer processing_timer;
            processing_timer.start();

            bool should_save_event_clip = false;
            bool loitering_detected = false;

            DetectionResult detection_result = detector.detect(frame);
            telemetry_inferences++;
            telemetry_preprocess_ms += detection_result.preprocess_ms;
            telemetry_inference_ms += detection_result.inference_ms;
            telemetry_postprocess_ms += detection_result.postprocess_ms;
            tracked_detections = tracker.update(detection_result.detections);

            const double current_time_sec = app_timer.elapsedMilliseconds() / 1000.0;

            std::vector<std::string> frame_events =
                event_engine.update(tracked_detections, current_time_sec);

            for (const auto& event : frame_events) {
                std::cout << event << "\n";
                kpi_writer.writeEvent("scene", event);

                if (isImportantForUi(event)) {
                    recent_events.push_back(event);
                }
            }

            std::vector<ZoneEvent> zone_events =
                zone_manager.update(tracked_detections, current_time_sec);

            for (const auto& zone_event : zone_events) {
                std::cout << zone_event.message << "\n";
                kpi_writer.writeEvent("zone", zone_event.message);

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
                clip_path << "event_" << std::setw(4) << std::setfill('0') << clip_id++
                          << "_t" << static_cast<int>(current_time_sec) << ".avi";
                const std::filesystem::path output_path =
                    std::filesystem::path(clip_dir) / clip_path.str();

                if (clip_buffer.saveToVideo(output_path.string(), 10.0)) {
                    std::cout << "[Sentinel] Event clip saved: "
                              << output_path.string() << "\n";
                    kpi_writer.writeEvent("clip", output_path.string());
                    enforceClipRetention(clip_dir, max_clips);
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

            if (loitering_detected &&
                (current_time_sec - last_reasoning_request_time_sec) >=
                    static_cast<double>(reasoning_cooldown_sec)) {
                kpi_writer.writeReasoningRequest("loitering", scene_json);
                std::cout << "[Reasoning] Queued loitering SceneState.\n";
                last_reasoning_request_time_sec = current_time_sec;
            }

            if ((current_time_sec - last_scene_print_time_sec) >=
                static_cast<double>(scene_interval_sec)) {
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

        const double telemetry_now_sec = app_timer.elapsedMilliseconds() / 1000.0;
        const double telemetry_elapsed_sec = telemetry_now_sec - telemetry_window_start_sec;
        if (telemetry_elapsed_sec >= static_cast<double>(telemetry_interval_sec)) {
            const double capture_fps =
                static_cast<double>(telemetry_frames) / telemetry_elapsed_sec;
            const double detection_fps =
                static_cast<double>(telemetry_inferences) / telemetry_elapsed_sec;
            const double inference_count = static_cast<double>(telemetry_inferences);
            const CaptureDiagnostics capture_diagnostics = video_source.takeDiagnostics();
            const double source_fps =
                static_cast<double>(capture_diagnostics.successful_reads) / telemetry_elapsed_sec;
            const double capture_delivery_percent =
                source_fps > 0.0 ? std::min(100.0, capture_fps / source_fps * 100.0) : 0.0;
            const double capture_slow_read_percent =
                capture_diagnostics.reads > 0
                    ? static_cast<double>(capture_diagnostics.slow_reads) /
                          static_cast<double>(capture_diagnostics.reads) * 100.0
                    : 0.0;

            std::cout << std::fixed << std::setprecision(2)
                      << "[Telemetry] source_fps=" << source_fps
                      << " capture_fps=" << capture_fps
                      << " capture_delivery_percent=" << capture_delivery_percent
                      << " detection_fps=" << detection_fps
                      << " preprocess_ms="
                      << (telemetry_inferences > 0 ? telemetry_preprocess_ms / inference_count : 0.0)
                      << " inference_ms="
                      << (telemetry_inferences > 0 ? telemetry_inference_ms / inference_count : 0.0)
                      << " postprocess_ms="
                      << (telemetry_inferences > 0 ? telemetry_postprocess_ms / inference_count : 0.0)
                      << " persons=" << person_detections.size()
                      << " rtsp_reconnects=" << video_source.reconnectCount()
                      << " last_frame_age_ms=" << video_source.lastFrameAgeMilliseconds()
                      << " capture_read_avg_ms=" << capture_diagnostics.average_read_ms
                      << " capture_read_max_ms=" << capture_diagnostics.max_read_ms
                      << " capture_slow_reads=" << capture_diagnostics.slow_reads
                      << " capture_slow_read_percent=" << capture_slow_read_percent
                      << "\n";

            kpi_writer.writeTelemetry({
                source_fps,
                capture_fps,
                capture_delivery_percent,
                detection_fps,
                telemetry_inferences > 0 ? telemetry_preprocess_ms / inference_count : 0.0,
                telemetry_inferences > 0 ? telemetry_inference_ms / inference_count : 0.0,
                telemetry_inferences > 0 ? telemetry_postprocess_ms / inference_count : 0.0,
                person_detections.size(),
                video_source.reconnectCount(),
                video_source.lastFrameAgeMilliseconds(),
                capture_diagnostics.average_read_ms,
                capture_diagnostics.max_read_ms,
                capture_diagnostics.slow_reads,
                capture_slow_read_percent
            });

            telemetry_window_start_sec = telemetry_now_sec;
            telemetry_frames = 0;
            telemetry_inferences = 0;
            telemetry_preprocess_ms = 0.0;
            telemetry_inference_ms = 0.0;
            telemetry_postprocess_ms = 0.0;
        }

        if (!headless) {
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
    }

    if (!headless) {
        cv::destroyAllWindows();
    }
    std::cout << "[Sentinel] Exiting run().\n";
    return true;
}
