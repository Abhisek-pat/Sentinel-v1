#include "detection/yolo_onnx.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::wstring toWideString(const std::string& input) {
    return std::wstring(input.begin(), input.end());
}
}  // namespace

YoloOnnxDetector::YoloOnnxDetector(const std::string& model_path)
    : model_path_(model_path),
      env_(ORT_LOGGING_LEVEL_WARNING, "Sentinel"),
      session_options_() {
}

bool YoloOnnxDetector::initialize() {
    try {
        session_options_.SetIntraOpNumThreads(1);
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

#ifdef _WIN32
        const std::wstring wide_model_path = toWideString(model_path_);
        session_ = std::make_unique<Ort::Session>(
            env_,
            wide_model_path.c_str(),
            session_options_);
#else
        session_ = std::make_unique<Ort::Session>(
            env_,
            model_path_.c_str(),
            session_options_);
#endif

        printModelInfo();
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "[Sentinel] ONNX Runtime initialization failed: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[Sentinel] Detector initialization failed: " << e.what() << "\n";
        return false;
    }
}

void YoloOnnxDetector::printModelInfo() {
    if (!session_) {
        return;
    }

    Ort::AllocatorWithDefaultOptions allocator;

    const std::size_t input_count = session_->GetInputCount();
    const std::size_t output_count = session_->GetOutputCount();

    std::cout << "[Sentinel] Model loaded: " << model_path_ << "\n";
    std::cout << "[Sentinel] Input count: " << input_count << "\n";
    std::cout << "[Sentinel] Output count: " << output_count << "\n";

    input_names_.clear();
    output_names_.clear();
    input_shape_.clear();

    for (std::size_t i = 0; i < input_count; ++i) {
        auto name_alloc = session_->GetInputNameAllocated(i, allocator);
        const char* input_name = name_alloc.get();

        input_names_.emplace_back(input_name);

        auto type_info = session_->GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();

        std::cout << "[Sentinel] Input[" << i << "] name: " << input_name << "\n";
        std::cout << "[Sentinel] Input[" << i << "] shape: ";

        for (std::size_t j = 0; j < shape.size(); ++j) {
            std::cout << shape[j];
            if (j + 1 < shape.size()) {
                std::cout << " x ";
            }
        }
        std::cout << "\n";

        if (i == 0) {
            input_shape_ = shape;
        }
    }

    for (std::size_t i = 0; i < output_count; ++i) {
        auto name_alloc = session_->GetOutputNameAllocated(i, allocator);
        const char* output_name = name_alloc.get();

        output_names_.emplace_back(output_name);

        auto type_info = session_->GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        auto shape = tensor_info.GetShape();

        std::cout << "[Sentinel] Output[" << i << "] name: " << output_name << "\n";
        std::cout << "[Sentinel] Output[" << i << "] shape: ";

        for (std::size_t j = 0; j < shape.size(); ++j) {
            std::cout << shape[j];
            if (j + 1 < shape.size()) {
                std::cout << " x ";
            }
        }
        std::cout << "\n";
    }
}

DetectionResult YoloOnnxDetector::detect(const cv::Mat& frame) {
    (void)frame;
    return {};
}