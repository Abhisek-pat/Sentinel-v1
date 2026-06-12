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
    bool hasEnded() const;

    double fps() const;
    int width() const;
    int height() const;
    std::uint64_t reconnectCount() const;
    std::int64_t lastFrameAgeMilliseconds() const;

private:
    bool isWebcamSource() const;
    int webcamIndex() const;
    bool isRtspSource() const;
    bool openInternal();
    bool reconnectRtsp();
    void captureLoop();

private:
    std::string source_;
    cv::VideoCapture cap_;

    std::thread capture_thread_;
    std::mutex frame_mutex_;
    cv::Mat latest_frame_;

    std::atomic<bool> running_{false};
    std::atomic<bool> frame_ready_{false};
    std::atomic<bool> end_of_stream_{false};
    std::atomic<std::uint64_t> reconnect_count_{0};
    std::atomic<std::int64_t> last_frame_time_ms_{-1};

    std::uint64_t latest_frame_id_{0};
    std::uint64_t delivered_frame_id_{0};
};
