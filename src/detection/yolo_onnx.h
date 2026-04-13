#pragma once

#include "detection/detector.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <vector>

class YoloOnnxDetector {
public:
    explicit YoloOnnxDetector(const std::string& model_path);

    bool initialize();
    DetectionResult detect(const cv::Mat& frame);

private:
    struct LetterboxInfo {
        std::vector<float> input_tensor_values;
        float scale{1.0f};
        int pad_x{0};
        int pad_y{0};
        int resized_width{0};
        int resized_height{0};
    };

    void printModelInfo();
    LetterboxInfo preprocess(const cv::Mat& frame) const;

    std::vector<Detection> decodeDetections(const float* output_data,
                                            std::size_t output_count,
                                            int original_width,
                                            int original_height,
                                            float scale,
                                            int pad_x,
                                            int pad_y) const;

private:
    std::string model_path_;

    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;

    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<int64_t> input_shape_;

    float conf_threshold_{0.35f};
    float nms_threshold_{0.45f};

    std::vector<std::string> class_names_;
};