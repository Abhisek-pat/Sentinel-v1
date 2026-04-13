#pragma once

#include "detection/detector.h"

#include <string>
#include <unordered_map>
#include <vector>

class EventEngine {
public:
    EventEngine();

    std::vector<std::string> update(std::vector<Detection>& detections,
                                    double current_time_sec);

private:
    struct TrackEventState {
        int track_id{-1};
        std::string class_name{"unknown"};

        double first_seen_sec{0.0};
        double last_seen_sec{0.0};

        bool entry_emitted{false};
        bool exit_emitted{false};
        bool currently_visible{false};
    };

private:
    std::unordered_map<int, TrackEventState> states_;

    // Minimum time a track must exist before we emit "entered"
    double entry_min_duration_sec_{1.0};

    // Time after last seen before considering "exited"
    double exit_timeout_sec_{1.5};

    // Minimum dwell time required to emit exit event
    double min_exit_dwell_sec_{2.0};
};