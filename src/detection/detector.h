#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

struct Detection {
    int class_id;
    std::string class_name;
    float confidence;
    cv::Rect box;
};

struct DetectionResult {
    std::vector<Detection> detections;
    double preprocess_ms{0.0};
    double inference_ms{0.0};
    double postprocess_ms{0.0};
};