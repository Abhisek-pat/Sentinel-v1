#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

struct TelemetryKpi {
    double source_fps{0.0};
    double capture_fps{0.0};
    double capture_delivery_percent{0.0};
    double detection_fps{0.0};
    double preprocess_ms{0.0};
    double inference_ms{0.0};
    double postprocess_ms{0.0};
    std::size_t persons{0};
    std::uint64_t rtsp_reconnects{0};
    std::int64_t last_frame_age_ms{-1};
    double capture_read_avg_ms{0.0};
    double capture_read_max_ms{0.0};
    std::uint64_t capture_slow_reads{0};
    double capture_slow_read_percent{0.0};
};

class KpiWriter {
public:
    explicit KpiWriter(const std::string& directory);

    bool enabled() const;
    void writeTelemetry(const TelemetryKpi& telemetry);
    void writeEvent(const std::string& category, const std::string& message);
    void writeReasoningRequest(const std::string& trigger,
                               const std::string& scene_state_json);

private:
    static std::int64_t unixTimeMilliseconds();
    static std::string escapeJson(const std::string& value);

    std::ofstream telemetry_stream_;
    std::ofstream event_stream_;
    std::ofstream reasoning_request_stream_;
};
