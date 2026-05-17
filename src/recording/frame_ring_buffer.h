#pragma once

#include <opencv2/opencv.hpp>

#include <mutex>
#include <string>
#include <vector>

class FrameRingBuffer {
public:
    explicit FrameRingBuffer(std::size_t max_frames);

    void push(const cv::Mat& frame);

    bool saveToVideo(const std::string& output_path,
                     double output_fps) const;

    std::size_t size() const;

private:
    std::vector<cv::Mat> snapshot() const;

private:
    std::size_t max_frames_{0};
    std::size_t write_index_{0};
    bool filled_{false};

    std::vector<cv::Mat> frames_;
    mutable std::mutex mutex_;
};