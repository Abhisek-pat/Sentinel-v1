#include "capture/video_source.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

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
    return source_.empty() || source_ == "0";
}

bool VideoSource::isRtspSource() const {
    return source_.rfind("rtsp://", 0) == 0;
}

bool VideoSource::openInternal() {
    if (isWebcamSource()) {
        std::cout << "[Sentinel] Opening webcam.\n";

#ifdef _WIN32
        cap_.open(0, cv::CAP_DSHOW);
#else
        cap_.open(0);
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

        cap_.open(source_, cv::CAP_FFMPEG);
        cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);

    } else {
        std::cout << "[Sentinel] Opening video file: " << source_ << "\n";
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

        if (cap_.read(frame) && !frame.empty()) {
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);

                latest_frame_ = frame;
                latest_frame_id_++;
                frame_ready_ = true;
            }
        } else {
            std::cerr << "[Sentinel] RTSP frame read failed in background thread. Retrying...\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool VideoSource::read(cv::Mat& frame) {
    if (!cap_.isOpened()) {
        return false;
    }

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

    return cap_.read(frame) && !frame.empty();
}

bool VideoSource::isOpened() const {
    return cap_.isOpened();
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