#pragma once

#include <opencv2/opencv.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

struct CaptureDiagnostics {
    std::uint64_t reads{0};
    std::uint64_t successful_reads{0};
    std::uint64_t slow_reads{0};
    double average_read_ms{0.0};
    double max_read_ms{0.0};
};

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
    CaptureDiagnostics takeDiagnostics();

private:
    bool isWebcamSource() const;
    int webcamIndex() const;
    bool isRtspSource() const;
    bool openInternal();
    bool reconnectRtsp();
    void captureLoop();
    void recordReadDuration(double duration_ms, bool successful);

private:
    std::string source_;
    cv::VideoCapture cap_;

    std::thread capture_thread_;
    std::mutex frame_mutex_;
    cv::Mat latest_frame_;
    std::mutex diagnostics_mutex_;
    std::uint64_t diagnostic_reads_{0};
    std::uint64_t diagnostic_successful_reads_{0};
    std::uint64_t diagnostic_slow_reads_{0};
    double diagnostic_total_read_ms_{0.0};
    double diagnostic_max_read_ms_{0.0};

    std::atomic<bool> running_{false};
    std::atomic<bool> frame_ready_{false};
    std::atomic<bool> end_of_stream_{false};
    std::atomic<std::uint64_t> reconnect_count_{0};
    std::atomic<std::int64_t> last_frame_time_ms_{-1};

    std::uint64_t latest_frame_id_{0};
    std::uint64_t delivered_frame_id_{0};
};
