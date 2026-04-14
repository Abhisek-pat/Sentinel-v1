#include "reasoning/scene_state.h"

#include <iomanip>
#include <sstream>

SceneState SceneStateBuilder::build(double timestamp_sec,
                                    const std::vector<Detection>& detections,
                                    const std::vector<std::string>& recent_events,
                                    const std::vector<std::string>& zones,
                                    const std::vector<bool>& loitering_flags) const {
    SceneState scene_state;
    scene_state.timestamp_sec = timestamp_sec;
    scene_state.recent_events = recent_events;

    std::size_t person_index = 0;

    for (const auto& det : detections) {
        if (det.class_name != "person") {
            continue;
        }

        PersonState person;
        person.track_id = det.track_id;
        person.confidence = det.confidence;
        person.x = det.box.x;
        person.y = det.box.y;
        person.width = det.box.width;
        person.height = det.box.height;
        person.dwell_time_sec = det.dwell_time_sec;

        if (person_index < zones.size()) {
            person.zone = zones[person_index];
        }

        if (person_index < loitering_flags.size()) {
            person.loitering = loitering_flags[person_index];
        }

        scene_state.persons.push_back(person);
        person_index++;
    }

    return scene_state;
}

std::string SceneStateBuilder::toJson(const SceneState& scene_state) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    oss << "{\n";
    oss << "  \"timestamp_sec\": " << scene_state.timestamp_sec << ",\n";

    oss << "  \"persons\": [\n";
    for (std::size_t i = 0; i < scene_state.persons.size(); ++i) {
        const auto& p = scene_state.persons[i];
        oss << "    {\n";
        oss << "      \"track_id\": " << p.track_id << ",\n";
        oss << "      \"confidence\": " << p.confidence << ",\n";
        oss << "      \"bbox\": [" << p.x << ", " << p.y << ", "
            << p.width << ", " << p.height << "],\n";
        oss << "      \"dwell_time_sec\": " << p.dwell_time_sec << ",\n";
        oss << "      \"zone\": \"" << p.zone << "\",\n";
        oss << "      \"loitering\": " << (p.loitering ? "true" : "false") << "\n";
        oss << "    }";
        if (i + 1 < scene_state.persons.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";

    oss << "  \"recent_events\": [\n";
    for (std::size_t i = 0; i < scene_state.recent_events.size(); ++i) {
        oss << "    \"" << scene_state.recent_events[i] << "\"";
        if (i + 1 < scene_state.recent_events.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ]\n";

    oss << "}\n";
    return oss.str();
}