struct Detection {
    int class_id;
    std::string class_name;
    float confidence;
    cv::Rect box;
};

struct DetectionResult {
    std::vector<Detection> detections;
    double preprocess_ms;
    double inference_ms;
    double postprocess_ms;
};