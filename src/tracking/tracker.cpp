#include "tracking/tracker.h"

#include <algorithm>

Tracker::Tracker() = default;

float Tracker::computeIoU(const cv::Rect& a, const cv::Rect& b) {
    const int intersection_x1 = std::max(a.x, b.x);
    const int intersection_y1 = std::max(a.y, b.y);
    const int intersection_x2 = std::min(a.x + a.width, b.x + b.width);
    const int intersection_y2 = std::min(a.y + a.height, b.y + b.height);

    const int intersection_width = std::max(0, intersection_x2 - intersection_x1);
    const int intersection_height = std::max(0, intersection_y2 - intersection_y1);
    const int intersection_area = intersection_width * intersection_height;

    const int union_area = a.area() + b.area() - intersection_area;

    if (union_area <= 0) {
        return 0.0f;
    }

    return static_cast<float>(intersection_area) / static_cast<float>(union_area);
}

std::vector<Detection> Tracker::update(const std::vector<Detection>& detections) {
    struct MatchCandidate {
        std::size_t track_index{0};
        std::size_t detection_index{0};
        float iou{0.0f};
    };

    std::vector<MatchCandidate> candidates;
    candidates.reserve(tracks_.size() * detections.size());

    for (std::size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
        for (std::size_t detection_index = 0;
             detection_index < detections.size();
             ++detection_index) {
            if (tracks_[track_index].class_id != detections[detection_index].class_id) {
                continue;
            }

            const float iou =
                computeIoU(tracks_[track_index].box, detections[detection_index].box);
            if (iou >= iou_threshold_) {
                candidates.push_back({track_index, detection_index, iou});
            }
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const MatchCandidate& left, const MatchCandidate& right) {
            return left.iou > right.iou;
        });

    std::vector<bool> track_used(tracks_.size(), false);
    std::vector<bool> detection_used(detections.size(), false);

    for (const auto& candidate : candidates) {
        if (track_used[candidate.track_index] ||
            detection_used[candidate.detection_index]) {
            continue;
        }

        auto& track = tracks_[candidate.track_index];
        const auto& detection = detections[candidate.detection_index];
        track.box = detection.box;
        track.confidence = detection.confidence;
        track.class_id = detection.class_id;
        track.class_name = detection.class_name;
        track.missing_frames = 0;

        track_used[candidate.track_index] = true;
        detection_used[candidate.detection_index] = true;
    }

    for (std::size_t track_index = 0; track_index < tracks_.size(); ++track_index) {
        if (!track_used[track_index]) {
            tracks_[track_index].missing_frames++;
        }
    }

    for (std::size_t i = 0; i < detections.size(); ++i) {
        if (detection_used[i]) {
            continue;
        }

        Track new_track;
        new_track.track_id = next_track_id_++;
        new_track.class_id = detections[i].class_id;
        new_track.class_name = detections[i].class_name;
        new_track.confidence = detections[i].confidence;
        new_track.box = detections[i].box;
        new_track.missing_frames = 0;
        tracks_.push_back(new_track);
    }

    tracks_.erase(
        std::remove_if(
            tracks_.begin(),
            tracks_.end(),
            [this](const Track& track) {
                return track.missing_frames > max_missing_frames_;
            }),
        tracks_.end());

    std::vector<Detection> tracked_detections;
    tracked_detections.reserve(tracks_.size());

    for (const auto& track : tracks_) {
        if (track.missing_frames > 0) {
            continue;
        }

        Detection det;
        det.track_id = track.track_id;
        det.class_id = track.class_id;
        det.class_name = track.class_name;
        det.confidence = track.confidence;
        det.box = track.box;
        tracked_detections.push_back(det);
    }

    return tracked_detections;
}
