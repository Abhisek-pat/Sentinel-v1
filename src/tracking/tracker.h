#pragma once

#include "detection/detector.h"

#include <opencv2/opencv.hpp>

#include <vector>

class Tracker {
public:
    Tracker();

    std::vector<Detection> update(const std::vector<Detection>& detections);

private:
    struct Track {
        int track_id{-1};
        int class_id{-1};
        std::string class_name{"unknown"};
        float confidence{0.0f};
        cv::Rect box;
        int missing_frames{0};
    };

    static float computeIoU(const cv::Rect& a, const cv::Rect& b);

private:
    std::vector<Track> tracks_;
    int next_track_id_{0};
    float iou_threshold_{0.3f};
    int max_missing_frames_{10};
};