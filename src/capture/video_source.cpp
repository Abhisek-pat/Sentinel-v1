#include "capture/video_source.h"

#include <iostream>

VideoSource::VideoSource(const std::string& source)
    : source_(source) {}

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
        std::cout << "[Sentinel] Opening RTSP stream: " << source_ << "\n";

        // Prefer FFmpeg for RTSP streams
        cap_.open(source_, cv::CAP_FFMPEG);

        // Best-effort latency reduction
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
    return openInternal();
}

bool VideoSource::read(cv::Mat& frame) {
    if (!cap_.isOpened()) {
        return false;
    }

    if (cap_.read(frame) && !frame.empty()) {
        return true;
    }

    // Try a simple reconnect for RTSP streams
    if (isRtspSource()) {
        std::cerr << "[Sentinel] RTSP read failed. Attempting reconnect...\n";
        cap_.release();

        if (!openInternal()) {
            std::cerr << "[Sentinel] RTSP reconnect failed.\n";
            return false;
        }

        if (cap_.read(frame) && !frame.empty()) {
            std::cout << "[Sentinel] RTSP reconnect successful.\n";
            return true;
        }

        std::cerr << "[Sentinel] RTSP reconnect read failed.\n";
    }

    return false;
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