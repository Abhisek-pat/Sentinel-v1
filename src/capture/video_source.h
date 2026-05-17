#pragma once

#include <opencv2/opencv.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class VideoSource {
public:
    explicit VideoSource(const std::string& source);
    ~VideoSource();

    bool open();
    bool read(cv::Mat& frame);
    bool isOpened() const;

    double fps() const;
    int width() const;
    int height() const;

private:
    bool isWebcamSource() const;
    bool isRtspSource() const;
    bool openInternal();
    void captureLoop();

private:
    std::string source_;
    cv::VideoCapture cap_;

    std::thread capture_thread_;
    std::mutex frame_mutex_;
    cv::Mat latest_frame_;

    std::atomic<bool> running_{false};
    std::atomic<bool> frame_ready_{false};

    std::uint64_t latest_frame_id_{0};
    std::uint64_t delivered_frame_id_{0};
};