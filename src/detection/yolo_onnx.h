#pragma once

#include "detection/detector.h"

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

class YoloOnnxDetector {
public:
    explicit YoloOnnxDetector(const std::string& model_path);

    bool initialize();
    DetectionResult detect(const cv::Mat& frame);

private:
    void printModelInfo();

private:
    std::string model_path_;

    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;

    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<int64_t> input_shape_;
};