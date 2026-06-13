#include "monitoring/kpi_writer.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

KpiWriter::KpiWriter(const std::string& directory) {
    if (directory.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "[KPI] Could not create directory " << directory
                  << ": " << error.message() << "\n";
        return;
    }

    telemetry_stream_.open(
        std::filesystem::path(directory) / "telemetry.jsonl",
        std::ios::app);
    event_stream_.open(
        std::filesystem::path(directory) / "events.jsonl",
        std::ios::app);
    reasoning_request_stream_.open(
        std::filesystem::path(directory) / "reasoning_requests.jsonl",
        std::ios::app);

    if (!enabled()) {
        std::cerr << "[KPI] Could not open KPI output files under "
                  << directory << "\n";
    }
}

bool KpiWriter::enabled() const {
    return telemetry_stream_.is_open() && event_stream_.is_open();
}

void KpiWriter::writeTelemetry(const TelemetryKpi& telemetry) {
    if (!enabled()) {
        return;
    }

    telemetry_stream_ << std::fixed << std::setprecision(2)
                      << "{\"timestamp_ms\":" << unixTimeMilliseconds()
                      << ",\"source_fps\":" << telemetry.source_fps
                      << ",\"capture_fps\":" << telemetry.capture_fps
                      << ",\"capture_delivery_percent\":" << telemetry.capture_delivery_percent
                      << ",\"detection_fps\":" << telemetry.detection_fps
                      << ",\"preprocess_ms\":" << telemetry.preprocess_ms
                      << ",\"inference_ms\":" << telemetry.inference_ms
                      << ",\"postprocess_ms\":" << telemetry.postprocess_ms
                      << ",\"persons\":" << telemetry.persons
                      << ",\"rtsp_reconnects\":" << telemetry.rtsp_reconnects
                      << ",\"last_frame_age_ms\":" << telemetry.last_frame_age_ms
                      << ",\"capture_read_avg_ms\":" << telemetry.capture_read_avg_ms
                      << ",\"capture_read_max_ms\":" << telemetry.capture_read_max_ms
                      << ",\"capture_slow_reads\":" << telemetry.capture_slow_reads
                      << ",\"capture_slow_read_percent\":" << telemetry.capture_slow_read_percent
                      << "}\n";
    telemetry_stream_.flush();
}

void KpiWriter::writeEvent(const std::string& category, const std::string& message) {
    if (!enabled()) {
        return;
    }

    event_stream_ << "{\"timestamp_ms\":" << unixTimeMilliseconds()
                  << ",\"category\":\"" << escapeJson(category)
                  << "\",\"message\":\"" << escapeJson(message)
                  << "\"}\n";
    event_stream_.flush();
}

void KpiWriter::writeReasoningRequest(const std::string& trigger,
                                      const std::string& scene_state_json) {
    if (!reasoning_request_stream_.is_open()) {
        return;
    }

    std::string compact_scene_state;
    compact_scene_state.reserve(scene_state_json.size());
    for (const char character : scene_state_json) {
        if (character != '\n' && character != '\r') {
            compact_scene_state.push_back(character);
        }
    }

    const std::int64_t timestamp_ms = unixTimeMilliseconds();
    reasoning_request_stream_ << "{\"request_id\":\"" << timestamp_ms
                              << "\",\"timestamp_ms\":" << timestamp_ms
                              << ",\"trigger\":\"" << escapeJson(trigger)
                              << "\",\"scene_state\":" << compact_scene_state
                              << "}\n";
    reasoning_request_stream_.flush();
}

std::int64_t KpiWriter::unixTimeMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string KpiWriter::escapeJson(const std::string& value) {
    std::ostringstream escaped;
    for (const char character : value) {
        switch (character) {
            case '\\':
                escaped << "\\\\";
                break;
            case '"':
                escaped << "\\\"";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                escaped << character;
                break;
        }
    }
    return escaped.str();
}
