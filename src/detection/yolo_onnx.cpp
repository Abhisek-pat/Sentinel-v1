#include "detection/yolo_onnx.h"

#include "utils/timer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
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
      session_options_(),
      class_names_({
          "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
          "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
          "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
          "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
          "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
          "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed",
          "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven",
          "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
      }) {
}

bool YoloOnnxDetector::initialize() {
    try {
        if (!std::filesystem::exists(model_path_)) {
            std::cerr << "[Sentinel] Model file does not exist: " << model_path_ << "\n";
            return false;
        }

        const auto file_size = std::filesystem::file_size(model_path_);
        std::cout << "[Sentinel] Model path: " << model_path_ << "\n";
        std::cout << "[Sentinel] Model size: " << file_size << " bytes\n";

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

YoloOnnxDetector::LetterboxInfo YoloOnnxDetector::preprocess(const cv::Mat& frame) const {
    LetterboxInfo info;

    const int target_width = 640;
    const int target_height = 640;

    const int original_width = frame.cols;
    const int original_height = frame.rows;

    const float scale_w = static_cast<float>(target_width) / static_cast<float>(original_width);
    const float scale_h = static_cast<float>(target_height) / static_cast<float>(original_height);
    info.scale = std::min(scale_w, scale_h);

    info.resized_width = static_cast<int>(std::round(original_width * info.scale));
    info.resized_height = static_cast<int>(std::round(original_height * info.scale));

    info.pad_x = (target_width - info.resized_width) / 2;
    info.pad_y = (target_height - info.resized_height) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(info.resized_width, info.resized_height));

    cv::Mat letterboxed(target_height, target_width, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(letterboxed(cv::Rect(info.pad_x, info.pad_y, info.resized_width, info.resized_height)));

    cv::Mat rgb;
    cv::cvtColor(letterboxed, rgb, cv::COLOR_BGR2RGB);

    cv::Mat float_image;
    rgb.convertTo(float_image, CV_32F, 1.0 / 255.0);

    const int channels = 3;
    info.input_tensor_values.resize(channels * target_height * target_width);

    std::vector<cv::Mat> chw(channels);
    for (int i = 0; i < channels; ++i) {
        chw[i] = cv::Mat(target_height, target_width, CV_32F,
                         info.input_tensor_values.data() + i * target_height * target_width);
    }

    cv::split(float_image, chw);

    return info;
}

std::vector<Detection> YoloOnnxDetector::decodeDetections(const float* output_data,
                                                          std::size_t output_count,
                                                          int original_width,
                                                          int original_height,
                                                          float scale,
                                                          int pad_x,
                                                          int pad_y) const {
    (void)output_count;

    const int num_classes = static_cast<int>(class_names_.size());
    const int num_predictions = 8400;
    const int dimensions = 4 + num_classes;

    if (dimensions != 84) {
        std::cerr << "[Sentinel] Unexpected YOLO output dimension count.\n";
        return {};
    }

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;

    boxes.reserve(num_predictions);
    confidences.reserve(num_predictions);
    class_ids.reserve(num_predictions);

    for (int i = 0; i < num_predictions; ++i) {
        const float cx = output_data[0 * num_predictions + i];
        const float cy = output_data[1 * num_predictions + i];
        const float w  = output_data[2 * num_predictions + i];
        const float h  = output_data[3 * num_predictions + i];

        float best_score = 0.0f;
        int best_class_id = -1;

        for (int class_id = 0; class_id < num_classes; ++class_id) {
            const float score = output_data[(4 + class_id) * num_predictions + i];
            if (score > best_score) {
                best_score = score;
                best_class_id = class_id;
            }
        }

        if (best_score < conf_threshold_) {
            continue;
        }

        float x1 = cx - 0.5f * w;
        float y1 = cy - 0.5f * h;
        float x2 = cx + 0.5f * w;
        float y2 = cy + 0.5f * h;

        x1 -= static_cast<float>(pad_x);
        y1 -= static_cast<float>(pad_y);
        x2 -= static_cast<float>(pad_x);
        y2 -= static_cast<float>(pad_y);

        x1 /= scale;
        y1 /= scale;
        x2 /= scale;
        y2 /= scale;

        const int left = std::max(0, static_cast<int>(std::round(x1)));
        const int top = std::max(0, static_cast<int>(std::round(y1)));
        const int right = std::min(original_width - 1, static_cast<int>(std::round(x2)));
        const int bottom = std::min(original_height - 1, static_cast<int>(std::round(y2)));

        const int box_width = right - left;
        const int box_height = bottom - top;

        if (box_width <= 0 || box_height <= 0) {
            continue;
        }

        boxes.emplace_back(left, top, box_width, box_height);
        confidences.push_back(best_score);
        class_ids.push_back(best_class_id);
    }

    std::vector<int> nms_indices;
    cv::dnn::NMSBoxes(boxes, confidences, conf_threshold_, nms_threshold_, nms_indices);

    std::vector<Detection> detections;
    detections.reserve(nms_indices.size());

    for (int idx : nms_indices) {
        Detection det;
        det.class_id = class_ids[idx];
        det.class_name = (det.class_id >= 0 && det.class_id < static_cast<int>(class_names_.size()))
            ? class_names_[det.class_id]
            : "unknown";
        det.confidence = confidences[idx];
        det.box = boxes[idx];
        detections.push_back(det);
    }

    return detections;
}

DetectionResult YoloOnnxDetector::detect(const cv::Mat& frame) {
    DetectionResult result;

    if (!session_) {
        std::cerr << "[Sentinel] detect() called before detector initialization.\n";
        return result;
    }

    Timer preprocess_timer;
    preprocess_timer.start();
    LetterboxInfo prep = preprocess(frame);
    result.preprocess_ms = preprocess_timer.elapsedMilliseconds();

    std::vector<int64_t> input_dims = {1, 3, 640, 640};

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        prep.input_tensor_values.data(),
        prep.input_tensor_values.size(),
        input_dims.data(),
        input_dims.size());

    std::vector<const char*> input_name_ptrs;
    input_name_ptrs.reserve(input_names_.size());
    for (const auto& name : input_names_) {
        input_name_ptrs.push_back(name.c_str());
    }

    std::vector<const char*> output_name_ptrs;
    output_name_ptrs.reserve(output_names_.size());
    for (const auto& name : output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }

    Timer inference_timer;
    inference_timer.start();

    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr},
        input_name_ptrs.data(),
        &input_tensor,
        1,
        output_name_ptrs.data(),
        output_name_ptrs.size());

    result.inference_ms = inference_timer.elapsedMilliseconds();

    if (output_tensors.empty() || !output_tensors[0].IsTensor()) {
        std::cerr << "[Sentinel] Inference returned no valid tensor output.\n";
        return result;
    }

    auto output_info = output_tensors[0].GetTensorTypeAndShapeInfo();
    const float* output_data = output_tensors[0].GetTensorData<float>();
    const std::size_t output_count = output_info.GetElementCount();

    Timer postprocess_timer;
    postprocess_timer.start();
    result.detections = decodeDetections(
        output_data,
        output_count,
        frame.cols,
        frame.rows,
        prep.scale,
        prep.pad_x,
        prep.pad_y);
    result.postprocess_ms = postprocess_timer.elapsedMilliseconds();

    return result;
}