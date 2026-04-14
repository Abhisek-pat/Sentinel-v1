#pragma once

#include "detection/detector.h"

#include <string>
#include <vector>

struct PersonState {
    int track_id{-1};
    float confidence{0.0f};
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    double dwell_time_sec{0.0};
    std::string zone{"NONE"};
    bool loitering{false};
};

struct SceneState {
    double timestamp_sec{0.0};
    std::vector<PersonState> persons;
    std::vector<std::string> recent_events;
};

class SceneStateBuilder {
public:
    SceneState build(double timestamp_sec,
                     const std::vector<Detection>& detections,
                     const std::vector<std::string>& recent_events,
                     const std::vector<std::string>& zones,
                     const std::vector<bool>& loitering_flags) const;

    std::string toJson(const SceneState& scene_state) const;
};