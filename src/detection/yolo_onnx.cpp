#include "detection/yolo_onnx.h"

#include "utils/timer.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int YOLO_INPUT_WIDTH = 320;
constexpr int YOLO_INPUT_HEIGHT = 320;
constexpr int YOLO_NUM_CLASSES = 80;

const std::vector<std::string> COCO_CLASSES = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck",
    "boat", "traffic light", "fire hydrant", "stop sign", "parking meter", "bench",
    "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra",
    "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove",
    "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup",
    "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
    "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink",
    "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"
};

bool fileExists(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

std::string shapeToString(const std::vector<int64_t>& shape) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        oss << shape[i];
        if (i + 1 < shape.size()) {
            oss << " x ";
        }
    }
    return oss.str();
}

cv::Rect clampRect(const cv::Rect& rect, int width, int height) {
    const int x1 = std::max(0, rect.x);
    const int y1 = std::max(0, rect.y);
    const int x2 = std::min(width - 1, rect.x + rect.width);
    const int y2 = std::min(height - 1, rect.y + rect.height);

    const int w = std::max(0, x2 - x1);
    const int h = std::max(0, y2 - y1);

    return cv::Rect(x1, y1, w, h);
}

}  // namespace

YoloOnnxDetector::YoloOnnxDetector(const std::string& model_path)
    : model_path_(model_path),
      env_(ORT_LOGGING_LEVEL_WARNING, "Sentinel") {}

bool YoloOnnxDetector::initialize() {
    std::cout << "[Sentinel] Model path: " << model_path_ << "\n";

    if (!fileExists(model_path_)) {
        std::cerr << "[Sentinel] Model file not found: " << model_path_ << "\n";
        return false;
    }

    std::ifstream model_file(model_path_, std::ios::binary | std::ios::ate);
    std::cout << "[Sentinel] Model size: " << model_file.tellg() << " bytes\n";

    try {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(2);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

#ifdef _WIN32
        std::wstring wide_model_path(model_path_.begin(), model_path_.end());
        session_ = std::make_unique<Ort::Session>(
            env_,
            wide_model_path.c_str(),
            session_options
        );
#else
        session_ = std::make_unique<Ort::Session>(
            env_,
            model_path_.c_str(),
            session_options
        );
#endif

        allocator_ = std::make_unique<Ort::AllocatorWithDefaultOptions>();

        input_names_.clear();
        output_names_.clear();
        input_name_ptrs_.clear();
        output_name_ptrs_.clear();

        const std::size_t input_count = session_->GetInputCount();
        const std::size_t output_count = session_->GetOutputCount();

        std::cout << "[Sentinel] Model loaded: " << model_path_ << "\n";
        std::cout << "[Sentinel] Input count: " << input_count << "\n";
        std::cout << "[Sentinel] Output count: " << output_count << "\n";

        for (std::size_t i = 0; i < input_count; ++i) {
            auto input_name = session_->GetInputNameAllocated(i, *allocator_);
            input_names_.push_back(input_name.get());

            auto type_info = session_->GetInputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();

            std::cout << "[Sentinel] Input[" << i << "] name: " << input_names_.back() << "\n";
            std::cout << "[Sentinel] Input[" << i << "] shape: " << shapeToString(shape) << "\n";
        }

        for (std::size_t i = 0; i < output_count; ++i) {
            auto output_name = session_->GetOutputNameAllocated(i, *allocator_);
            output_names_.push_back(output_name.get());

            auto type_info = session_->GetOutputTypeInfo(i);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto shape = tensor_info.GetShape();

            std::cout << "[Sentinel] Output[" << i << "] name: " << output_names_.back() << "\n";
            std::cout << "[Sentinel] Output[" << i << "] shape: " << shapeToString(shape) << "\n";
        }

        for (const auto& name : input_names_) {
            input_name_ptrs_.push_back(name.c_str());
        }

        for (const auto& name : output_names_) {
            output_name_ptrs_.push_back(name.c_str());
        }

        initialized_ = true;
        return true;

    } catch (const Ort::Exception& e) {
        std::cerr << "[Sentinel] ONNX Runtime initialization failed: "
                  << e.what() << "\n";
        return false;
    }
}

DetectionResult YoloOnnxDetector::detect(const cv::Mat& frame) {
    DetectionResult result;

    if (!initialized_ || !session_) {
        std::cerr << "[Sentinel] Detector not initialized.\n";
        return result;
    }

    if (frame.empty()) {
        std::cerr << "[Sentinel] Empty frame passed to detector.\n";
        return result;
    }

    try {
        Timer preprocess_timer;
        preprocess_timer.start();

       LetterboxInfo letterbox = preprocess(frame);
       std::vector<float> input_tensor_values = matToTensor(letterbox.input_image);

        result.preprocess_ms = preprocess_timer.elapsedMilliseconds();

        std::vector<int64_t> input_shape = {
            1,
            3,
            YOLO_INPUT_HEIGHT,
            YOLO_INPUT_WIDTH
        };

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator,
            OrtMemTypeDefault
        );

        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            input_shape.data(),
            input_shape.size()
        );

        Timer inference_timer;
        inference_timer.start();

        auto output_tensors = session_->Run(
            Ort::RunOptions{nullptr},
            input_name_ptrs_.data(),
            &input_tensor,
            1,
            output_name_ptrs_.data(),
            output_name_ptrs_.size()
        );

        result.inference_ms = inference_timer.elapsedMilliseconds();

        if (output_tensors.empty() || !output_tensors[0].IsTensor()) {
            std::cerr << "[Sentinel] Inference returned no valid tensor output.\n";
            return result;
        }

        auto output_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        const std::size_t output_count = output_info.GetElementCount();
        std::vector<int64_t> output_shape = output_info.GetShape();

        Timer postprocess_timer;
        postprocess_timer.start();

        result.detections = decodeDetections(
            output_data,
            output_count,
            output_shape,
            frame.cols,
            frame.rows,
            letterbox.scale,
            letterbox.pad_x,
            letterbox.pad_y
        );

        result.postprocess_ms = postprocess_timer.elapsedMilliseconds();

    } catch (const Ort::Exception& e) {
        std::cerr << "[Sentinel] ONNX Runtime detect failed: "
                  << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[Sentinel] Detection failed: "
                  << e.what() << "\n";
    }

    return result;
}

YoloOnnxDetector::LetterboxInfo YoloOnnxDetector::preprocess(const cv::Mat& frame) const {
    LetterboxInfo info;

    constexpr int YOLO_INPUT_WIDTH = 320;
    constexpr int YOLO_INPUT_HEIGHT = 320;

    const int frame_width = frame.cols;
    const int frame_height = frame.rows;

    info.scale = std::min(
        static_cast<float>(YOLO_INPUT_WIDTH) / static_cast<float>(frame_width),
        static_cast<float>(YOLO_INPUT_HEIGHT) / static_cast<float>(frame_height)
    );

    const int resized_width = static_cast<int>(std::round(frame_width * info.scale));
    const int resized_height = static_cast<int>(std::round(frame_height * info.scale));

    info.pad_x = (YOLO_INPUT_WIDTH - resized_width) / 2;
    info.pad_y = (YOLO_INPUT_HEIGHT - resized_height) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(resized_width, resized_height));

    info.input_image = cv::Mat(
        YOLO_INPUT_HEIGHT,
        YOLO_INPUT_WIDTH,
        CV_8UC3,
        cv::Scalar(114, 114, 114)
    );

    resized.copyTo(info.input_image(cv::Rect(
        info.pad_x,
        info.pad_y,
        resized_width,
        resized_height
    )));

    return info;
}

std::vector<Detection> YoloOnnxDetector::decodeDetections(
    const float* output_data,
    std::size_t output_count,
    const std::vector<int64_t>& output_shape,
    int frame_width,
    int frame_height,
    float scale,
    int pad_x,
    int pad_y) const {
    std::vector<Detection> detections;

    if (output_data == nullptr || output_shape.size() != 3) {
        std::cerr << "[Sentinel] Invalid YOLO output.\n";
        return detections;
    }

    const int64_t dim0 = output_shape[0];
    const int64_t dim1 = output_shape[1];
    const int64_t dim2 = output_shape[2];

    if (dim0 != 1) {
        std::cerr << "[Sentinel] Unsupported batch size: " << dim0 << "\n";
        return detections;
    }

    int channels = 0;
    int num_candidates = 0;

    // Expected YOLOv8 format: [1, 84, 2100] or [1, 84, 8400]
    if (dim1 == 84) {
        channels = static_cast<int>(dim1);
        num_candidates = static_cast<int>(dim2);
    }
    // Some exports may be transposed: [1, 2100, 84]
    else if (dim2 == 84) {
        channels = static_cast<int>(dim2);
        num_candidates = static_cast<int>(dim1);
    } else {
        std::cerr << "[Sentinel] Unsupported YOLO output shape: "
                  << shapeToString(output_shape) << "\n";
        return detections;
    }

    const int num_classes = channels - 4;

    if (num_classes <= 0 || num_classes > YOLO_NUM_CLASSES) {
        std::cerr << "[Sentinel] Invalid class count: " << num_classes << "\n";
        return detections;
    }

    if (output_count < static_cast<std::size_t>(channels * num_candidates)) {
        std::cerr << "[Sentinel] Output tensor smaller than expected.\n";
        return detections;
    }

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    boxes.reserve(num_candidates);
    scores.reserve(num_candidates);
    class_ids.reserve(num_candidates);

    const bool channel_first = (dim1 == 84);

    for (int i = 0; i < num_candidates; ++i) {
        float cx = 0.0f;
        float cy = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        if (channel_first) {
            cx = output_data[0 * num_candidates + i];
            cy = output_data[1 * num_candidates + i];
            w  = output_data[2 * num_candidates + i];
            h  = output_data[3 * num_candidates + i];
        } else {
            cx = output_data[i * channels + 0];
            cy = output_data[i * channels + 1];
            w  = output_data[i * channels + 2];
            h  = output_data[i * channels + 3];
        }

        int best_class_id = -1;
        float best_score = 0.0f;

        for (int c = 0; c < num_classes; ++c) {
            float class_score = 0.0f;

            if (channel_first) {
                class_score = output_data[(4 + c) * num_candidates + i];
            } else {
                class_score = output_data[i * channels + 4 + c];
            }

            if (class_score > best_score) {
                best_score = class_score;
                best_class_id = c;
            }
        }

        // YOLOv8 has no objectness channel.
        // Confidence is directly the best class score.
        if (best_score < conf_threshold_) {
            continue;
        }

        // For your project, keep person-only detection.
        // COCO person class = 0.
        if (best_class_id != 0) {
            continue;
        }

        const float x1_model = cx - (w / 2.0f);
        const float y1_model = cy - (h / 2.0f);
        const float x2_model = cx + (w / 2.0f);
        const float y2_model = cy + (h / 2.0f);

        const float x1 = (x1_model - static_cast<float>(pad_x)) / scale;
        const float y1 = (y1_model - static_cast<float>(pad_y)) / scale;
        const float x2 = (x2_model - static_cast<float>(pad_x)) / scale;
        const float y2 = (y2_model - static_cast<float>(pad_y)) / scale;

        cv::Rect box(
            static_cast<int>(std::round(x1)),
            static_cast<int>(std::round(y1)),
            static_cast<int>(std::round(x2 - x1)),
            static_cast<int>(std::round(y2 - y1))
        );

        box = clampRect(box, frame_width, frame_height);

        if (box.width <= 2 || box.height <= 2) {
            continue;
        }

        boxes.push_back(box);
        scores.push_back(best_score);
        class_ids.push_back(best_class_id);
    }

    std::vector<int> nms_indices;
    cv::dnn::NMSBoxes(
        boxes,
        scores,
        conf_threshold_,
        nms_threshold_,
        nms_indices
    );

    detections.reserve(nms_indices.size());

    for (int idx : nms_indices) {
        Detection det;
        det.class_id = class_ids[idx];
        det.class_name = COCO_CLASSES[class_ids[idx]];
        det.confidence = scores[idx];
        det.box = boxes[idx];
        det.track_id = -1;
        det.dwell_time_sec = 0.0;

        detections.push_back(det);
    }

    return detections;
}

std::vector<float> YoloOnnxDetector::matToTensor(const cv::Mat& image) const {
    constexpr int YOLO_INPUT_WIDTH = 320;
    constexpr int YOLO_INPUT_HEIGHT = 320;

    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> tensor(1 * 3 * YOLO_INPUT_HEIGHT * YOLO_INPUT_WIDTH);

    const int image_area = YOLO_INPUT_HEIGHT * YOLO_INPUT_WIDTH;

    for (int y = 0; y < YOLO_INPUT_HEIGHT; ++y) {
        for (int x = 0; x < YOLO_INPUT_WIDTH; ++x) {
            const cv::Vec3f& pixel = rgb.at<cv::Vec3f>(y, x);

            tensor[0 * image_area + y * YOLO_INPUT_WIDTH + x] = pixel[0];
            tensor[1 * image_area + y * YOLO_INPUT_WIDTH + x] = pixel[1];
            tensor[2 * image_area + y * YOLO_INPUT_WIDTH + x] = pixel[2];
        }
    }

    return tensor;
}