#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct Detection {
    int track_id{-1};
    int class_id{-1};
    std::string class_name{"unknown"};
    float confidence{0.0f};
    cv::Rect box;
    double dwell_time_sec{0.0};
};

struct DetectionResult {
    std::vector<Detection> detections;
    double preprocess_ms{0.0};
    double inference_ms{0.0};
    double postprocess_ms{0.0};
};