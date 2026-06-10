#include "zones/zone_manager.h"

#include <iomanip>
#include <sstream>

ZoneManager::ZoneManager() = default;

void ZoneManager::initialize(int frame_width, int frame_height) {
    zones_.clear();

    zones_.push_back({
        "CENTER_ZONE",
        cv::Rect(frame_width / 4, frame_height / 4,
                 frame_width / 2, frame_height / 2)
    });

    zones_.push_back({
        "LEFT_ZONE",
        cv::Rect(0, 0, frame_width / 4, frame_height)
    });
}

bool ZoneManager::isInside(const cv::Rect& box, const cv::Rect& zone) const {
    cv::Point center(box.x + box.width / 2, box.y + box.height / 2);
    return zone.contains(center);
}

std::vector<ZoneEvent> ZoneManager::update(const std::vector<Detection>& detections,
                                           double current_time_sec) {
    std::vector<ZoneEvent> events;

    for (const auto& det : detections) {
        if (det.class_name != "person") {
            continue;
        }

        for (const auto& zone : zones_) {
            bool inside = isInside(det.box, zone.area);

            auto& zone_map = zone_states_[zone.name];
            auto it = zone_map.find(det.track_id);

            if (inside) {
                if (it == zone_map.end()) {
                    ZoneState state;
                    state.track_id = det.track_id;
                    state.enter_time = current_time_sec;
                    state.last_seen_time = current_time_sec;
                    state.inside = true;
                    state.loitering_triggered = false;

                    zone_map[det.track_id] = state;

                    std::ostringstream oss;
                    oss << "[Zone] Track " << det.track_id
                        << " entered " << zone.name;
                    events.push_back({oss.str()});
                } else {
                    auto& state = it->second;
                    state.last_seen_time = current_time_sec;

                    const double dwell = current_time_sec - state.enter_time;

                    if (dwell > loiter_threshold_sec_ && !state.loitering_triggered) {
                        std::ostringstream oss;
                        oss << "[Zone] Track " << det.track_id
                            << " loitering in " << zone.name
                            << " for " << dwell << "s";
                        events.push_back({oss.str()});

                        state.loitering_triggered = true;
                    }
                }
            } else {
                // Keep the state briefly so boundary jitter does not emit
                // repeated exit and entry events.
            }
        }
    }

    for (const auto& zone : zones_) {
        auto& zone_map = zone_states_[zone.name];

        for (auto it = zone_map.begin(); it != zone_map.end();) {
            const double time_since_inside = current_time_sec - it->second.last_seen_time;
            if (it->second.inside && time_since_inside >= exit_timeout_sec_) {
                const double dwell = it->second.last_seen_time - it->second.enter_time;

                std::ostringstream oss;
                oss << "[Zone] Track " << it->first
                    << " exited " << zone.name
                    << " after " << std::fixed << std::setprecision(1)
                    << dwell << "s";
                events.push_back({oss.str()});

                it = zone_map.erase(it);
            } else {
                ++it;
            }
        }
    }

    return events;
}

void ZoneManager::drawZones(cv::Mat& frame) const {
    for (const auto& zone : zones_) {
        cv::rectangle(frame, zone.area, cv::Scalar(255, 0, 0), 2);

        cv::putText(frame,
                    zone.name,
                    cv::Point(zone.area.x, zone.area.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(255, 0, 0),
                    1);
    }
}

std::string ZoneManager::getZoneForTrack(int track_id) const {
    for (const auto& [zone_name, zone_map] : zone_states_) {
        auto it = zone_map.find(track_id);
        if (it != zone_map.end() && it->second.inside) {
            return zone_name;
        }
    }
    return "NONE";
}

bool ZoneManager::isTrackLoitering(int track_id) const {
    for (const auto& [zone_name, zone_map] : zone_states_) {
        auto it = zone_map.find(track_id);
        if (it != zone_map.end() && it->second.inside && it->second.loitering_triggered) {
            return true;
        }
    }
    return false;
}
