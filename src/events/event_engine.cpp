#include "events/event_engine.h"

#include <iomanip>
#include <sstream>
#include <unordered_set>

EventEngine::EventEngine() = default;

std::vector<std::string> EventEngine::update(std::vector<Detection>& detections, double current_time_sec) {
    std::vector<std::string> events;
    std::unordered_set<int> visible_track_ids;

    for (auto& detection : detections) {
        if (detection.class_name != "person") {
            continue;
        }

        if (detection.track_id < 0) {
            continue;
        }

        visible_track_ids.insert(detection.track_id);

        auto it = states_.find(detection.track_id);
        if (it == states_.end()) {
            TrackEventState state;
            state.track_id = detection.track_id;
            state.class_name = detection.class_name;
            state.first_seen_sec = current_time_sec;
            state.last_seen_sec = current_time_sec;
            state.entry_emitted = false;
            state.exit_emitted = false;
            state.currently_visible = true;

            states_[detection.track_id] = state;
            detection.dwell_time_sec = 0.0;
        } else {
            auto& state = it->second;
            state.last_seen_sec = current_time_sec;
            state.currently_visible = true;
            state.class_name = detection.class_name;
            state.exit_emitted = false;

            detection.dwell_time_sec = current_time_sec - state.first_seen_sec;

            if (!state.entry_emitted && detection.dwell_time_sec >= entry_min_duration_sec_) {
                std::ostringstream oss;
                oss << "[Event] Track " << detection.track_id
                    << " (" << detection.class_name << ") entered scene";
                events.push_back(oss.str());
                state.entry_emitted = true;
            }
        }
    }

    for (auto& [track_id, state] : states_) {
        if (visible_track_ids.find(track_id) != visible_track_ids.end()) {
            continue;
        }

        const double dwell = state.last_seen_sec - state.first_seen_sec;
        const double time_since_seen = current_time_sec - state.last_seen_sec;

        if (state.currently_visible &&
            !state.exit_emitted &&
            time_since_seen >= exit_timeout_sec_ &&
            dwell >= min_exit_dwell_sec_) {
            std::ostringstream oss;
            oss << "[Event] Track " << track_id
                << " (" << state.class_name << ") exited scene after "
                << std::fixed << std::setprecision(1)
                << dwell << "s";
            events.push_back(oss.str());

            state.currently_visible = false;
            state.exit_emitted = true;
        }
    }

    return events;
}