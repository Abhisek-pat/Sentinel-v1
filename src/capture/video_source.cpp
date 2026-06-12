#include "capture/video_source.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

std::int64_t steadyClockMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

VideoSource::VideoSource(const std::string& source)
    : source_(source) {}

VideoSource::~VideoSource() {
    running_ = false;

    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool VideoSource::isWebcamSource() const {
    if (source_.empty()) {
        return true;
    }

    return source_.find_first_not_of("0123456789") == std::string::npos;
}

int VideoSource::webcamIndex() const {
    if (source_.empty()) {
        return 0;
    }

    return std::stoi(source_);
}

bool VideoSource::isRtspSource() const {
    return source_.rfind("rtsp://", 0) == 0;
}

bool VideoSource::openInternal() {
    if (isWebcamSource()) {
        const int webcam_index = webcamIndex();
        std::cout << "[Sentinel] Opening webcam index " << webcam_index << ".\n";

#ifdef _WIN32
        cap_.open(webcam_index, cv::CAP_DSHOW);
#else
        cap_.open(webcam_index);
#endif

    } else if (isRtspSource()) {
        std::cout << "[Sentinel] Opening RTSP stream.\n";

#ifdef _WIN32
        _putenv_s(
            "OPENCV_FFMPEG_CAPTURE_OPTIONS",
            "rtsp_transport;tcp|fflags;nobuffer|flags;low_delay|max_delay;0"
        );
#else
        setenv(
            "OPENCV_FFMPEG_CAPTURE_OPTIONS",
            "rtsp_transport;tcp|fflags;nobuffer|flags;low_delay|max_delay;0",
            1
        );
#endif

        cap_.open(
            source_,
            cv::CAP_FFMPEG,
            {
                cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 5000,
                cv::CAP_PROP_READ_TIMEOUT_MSEC, 10000
            });
        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);

    } else {
        std::cout << "[Sentinel] Opening video file: " << source_ << "\n";

        std::error_code error;
        const std::filesystem::path video_path(source_);

        if (!std::filesystem::exists(video_path, error)) {
            std::cerr << "[Sentinel] Video file does not exist: " << source_ << "\n";
            return false;
        }

        if (std::filesystem::is_regular_file(video_path, error) &&
            std::filesystem::file_size(video_path, error) == 0) {
            std::cerr << "[Sentinel] Video file is empty: " << source_ << "\n";
            return false;
        }

        cap_.open(source_);
    }

    if (!cap_.isOpened()) {
        std::cerr << "[Sentinel] Failed to open source.\n";
        return false;
    }

    std::cout << "[Sentinel] Source opened successfully.\n";
    std::cout << "[Sentinel] Width: " << width()
              << ", Height: " << height()
              << ", FPS: " << fps() << "\n";

    return true;
}

bool VideoSource::reconnectRtsp() {
    frame_ready_ = false;
    cap_.release();

    while (running_) {
        std::cerr << "[Sentinel] Reconnecting RTSP stream.\n";
        if (openInternal()) {
            reconnect_count_++;
            std::cout << "[Sentinel] RTSP stream reconnected. Count: "
                      << reconnect_count_.load() << "\n";
            return true;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    return false;
}

bool VideoSource::open() {
    if (!openInternal()) {
        return false;
    }

    if (isRtspSource()) {
        running_ = true;
        capture_thread_ = std::thread(&VideoSource::captureLoop, this);
    }

    return true;
}

void VideoSource::captureLoop() {
    while (running_) {
        cv::Mat frame;
        const auto read_start = std::chrono::steady_clock::now();
        const bool read_ok = cap_.read(frame);
        const double read_duration_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - read_start)
                .count();
        recordReadDuration(read_duration_ms, read_ok && !frame.empty());

        if (read_ok && !frame.empty()) {
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);

                latest_frame_ = frame;
                latest_frame_id_++;
                frame_ready_ = true;
                last_frame_time_ms_ = steadyClockMilliseconds();
            }
        } else {
            std::cerr << "[Sentinel] RTSP frame read failed in background thread.\n";
            if (!reconnectRtsp()) {
                break;
            }
        }
    }
}

bool VideoSource::read(cv::Mat& frame) {
    if (isRtspSource()) {
        if (!frame_ready_) {
            return false;
        }

        std::lock_guard<std::mutex> lock(frame_mutex_);

        if (latest_frame_.empty()) {
            return false;
        }

        // Critical fix:
        // Do not return the same RTSP frame again and again.
        if (latest_frame_id_ == delivered_frame_id_) {
            return false;
        }

        frame = latest_frame_.clone();
        delivered_frame_id_ = latest_frame_id_;
        return true;
    }

    if (!cap_.isOpened()) {
        return false;
    }

    const auto read_start = std::chrono::steady_clock::now();
    const bool read_ok = cap_.read(frame) && !frame.empty();
    const double read_duration_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - read_start)
            .count();
    recordReadDuration(read_duration_ms, read_ok);

    if (!read_ok && !isWebcamSource()) {
        end_of_stream_ = true;
    }

    return read_ok;
}

bool VideoSource::isOpened() const {
    return cap_.isOpened();
}

bool VideoSource::hasEnded() const {
    return end_of_stream_;
}

double VideoSource::fps() const {
    return cap_.get(cv::CAP_PROP_FPS);
}

int VideoSource::width() const {
    return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
}

int VideoSource::height() const {
    return static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));
}

std::uint64_t VideoSource::reconnectCount() const {
    return reconnect_count_.load();
}

std::int64_t VideoSource::lastFrameAgeMilliseconds() const {
    const std::int64_t last_frame_time = last_frame_time_ms_.load();
    if (last_frame_time < 0) {
        return -1;
    }

    return steadyClockMilliseconds() - last_frame_time;
}

void VideoSource::recordReadDuration(double duration_ms, bool successful) {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);
    diagnostic_reads_++;
    if (successful) {
        diagnostic_successful_reads_++;
    }
    diagnostic_total_read_ms_ += duration_ms;
    diagnostic_max_read_ms_ = std::max(diagnostic_max_read_ms_, duration_ms);
    if (duration_ms >= 100.0) {
        diagnostic_slow_reads_++;
    }
}

CaptureDiagnostics VideoSource::takeDiagnostics() {
    std::lock_guard<std::mutex> lock(diagnostics_mutex_);

    CaptureDiagnostics diagnostics;
    diagnostics.reads = diagnostic_reads_;
    diagnostics.successful_reads = diagnostic_successful_reads_;
    diagnostics.slow_reads = diagnostic_slow_reads_;
    diagnostics.average_read_ms =
        diagnostic_reads_ > 0
            ? diagnostic_total_read_ms_ / static_cast<double>(diagnostic_reads_)
            : 0.0;
    diagnostics.max_read_ms = diagnostic_max_read_ms_;

    diagnostic_reads_ = 0;
    diagnostic_successful_reads_ = 0;
    diagnostic_slow_reads_ = 0;
    diagnostic_total_read_ms_ = 0.0;
    diagnostic_max_read_ms_ = 0.0;
    return diagnostics;
}
