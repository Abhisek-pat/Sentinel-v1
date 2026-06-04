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
        cv::Mat input_image;
        float scale{1.0f};
        int pad_x{0};
        int pad_y{0};
    };

    LetterboxInfo preprocess(const cv::Mat& frame) const;

    std::vector<float> matToTensor(const cv::Mat& image) const;

    std::vector<Detection> decodeDetections(
        const float* output_data,
        std::size_t output_count,
        const std::vector<int64_t>& output_shape,
        int frame_width,
        int frame_height,
        float scale,
        int pad_x,
        int pad_y) const;

private:
    std::string model_path_;

    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::AllocatorWithDefaultOptions> allocator_;

    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<const char*> input_name_ptrs_;
    std::vector<const char*> output_name_ptrs_;

    bool initialized_{false};

    float conf_threshold_{0.25f};
    float nms_threshold_{0.45f};
};