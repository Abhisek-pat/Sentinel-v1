#include "zones/zone_manager.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_set>

ZoneManager::ZoneManager() = default;

void ZoneManager::initialize(int frame_width, int frame_height) {
    zones_.clear();
    zone_states_.clear();
    entry_observations_.clear();

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

bool ZoneManager::centerInside(const cv::Rect& box, const cv::Rect& zone) {
    cv::Point center(box.x + box.width / 2, box.y + box.height / 2);
    return zone.contains(center);
}

cv::Rect ZoneManager::entryArea(const cv::Rect& zone) const {
    const int margin_x =
        std::min(zone.width / 3, std::max(4, static_cast<int>(zone.width * boundary_margin_ratio_)));
    const int margin_y =
        std::min(zone.height / 3, std::max(4, static_cast<int>(zone.height * boundary_margin_ratio_)));
    return cv::Rect(
        zone.x + margin_x,
        zone.y + margin_y,
        zone.width - 2 * margin_x,
        zone.height - 2 * margin_y);
}

cv::Rect ZoneManager::stayArea(const cv::Rect& zone) const {
    const int margin_x = std::max(4, static_cast<int>(zone.width * boundary_margin_ratio_));
    const int margin_y = std::max(4, static_cast<int>(zone.height * boundary_margin_ratio_));
    return cv::Rect(
        zone.x - margin_x,
        zone.y - margin_y,
        zone.width + 2 * margin_x,
        zone.height + 2 * margin_y);
}

std::vector<ZoneEvent> ZoneManager::update(const std::vector<Detection>& detections,
                                           double current_time_sec) {
    std::vector<ZoneEvent> events;
    std::unordered_set<int> visible_track_ids;

    for (const auto& det : detections) {
        if (det.class_name != "person") {
            continue;
        }
        visible_track_ids.insert(det.track_id);

        for (const auto& zone : zones_) {
            auto& zone_map = zone_states_[zone.name];
            auto& entry_map = entry_observations_[zone.name];
            auto it = zone_map.find(det.track_id);
            const bool clearly_inside = centerInside(det.box, entryArea(zone.area));
            const bool within_stay_area = centerInside(det.box, stayArea(zone.area));

            if (it == zone_map.end()) {
                if (clearly_inside) {
                    const int observations = ++entry_map[det.track_id];
                    if (observations < entry_confirmation_observations_) {
                        continue;
                    }

                    ZoneState state;
                    state.track_id = det.track_id;
                    state.enter_time = current_time_sec;
                    state.last_seen_time = current_time_sec;
                    state.inside = true;
                    state.loitering_triggered = false;

                    zone_map[det.track_id] = state;
                    entry_map.erase(det.track_id);

                    std::ostringstream oss;
                    oss << "[Zone] Track " << det.track_id
                        << " entered " << zone.name;
                    events.push_back({oss.str()});
                } else {
                    entry_map.erase(det.track_id);
                }
                continue;
            }

            auto& state = it->second;
            if (within_stay_area) {
                state.last_seen_time = current_time_sec;
                state.outside_observations = 0;

                const double dwell = current_time_sec - state.enter_time;
                if (dwell > loiter_threshold_sec_ && !state.loitering_triggered) {
                    std::ostringstream oss;
                    oss << "[Zone] Track " << det.track_id
                        << " loitering in " << zone.name
                        << " for " << dwell << "s";
                    events.push_back({oss.str()});
                    state.loitering_triggered = true;
                }
            } else {
                state.outside_observations++;
            }
        }
    }

    for (const auto& zone : zones_) {
        auto& zone_map = zone_states_[zone.name];
        auto& entry_map = entry_observations_[zone.name];

        for (auto it = entry_map.begin(); it != entry_map.end();) {
            if (visible_track_ids.find(it->first) == visible_track_ids.end()) {
                it = entry_map.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = zone_map.begin(); it != zone_map.end();) {
            if (visible_track_ids.find(it->first) == visible_track_ids.end()) {
                it->second.outside_observations++;
            }

            const double time_since_inside = current_time_sec - it->second.last_seen_time;
            if (it->second.inside &&
                it->second.outside_observations >= exit_confirmation_observations_ &&
                time_since_inside >= exit_timeout_sec_) {
                const double dwell = it->second.last_seen_time - it->second.enter_time;

                if (dwell >= min_exit_event_dwell_sec_) {
                    std::ostringstream oss;
                    oss << "[Zone] Track " << it->first
                        << " exited " << zone.name
                        << " after " << std::fixed << std::setprecision(1)
                        << dwell << "s";
                    events.push_back({oss.str()});
                }

                entry_map.erase(it->first);
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
