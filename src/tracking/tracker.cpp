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
    std::vector<bool> detection_used(detections.size(), false);

    for (auto& track : tracks_) {
        float best_iou = 0.0f;
        int best_detection_index = -1;

        for (std::size_t i = 0; i < detections.size(); ++i) {
            if (detection_used[i]) {
                continue;
            }

            if (track.class_id != detections[i].class_id) {
                continue;
            }

            const float iou = computeIoU(track.box, detections[i].box);
            if (iou > best_iou) {
                best_iou = iou;
                best_detection_index = static_cast<int>(i);
            }
        }

        if (best_detection_index >= 0 && best_iou >= iou_threshold_) {
            const auto& det = detections[best_detection_index];
            track.box = det.box;
            track.confidence = det.confidence;
            track.class_id = det.class_id;
            track.class_name = det.class_name;
            track.missing_frames = 0;
            detection_used[best_detection_index] = true;
        } else {
            track.missing_frames++;
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