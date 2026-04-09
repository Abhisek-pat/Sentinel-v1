#include "capture/video_source.h"

#include <iostream>

VideoSource::VideoSource(const std::string& source)
    : source_(source) {}

bool VideoSource::isWebcamSource() const {
    return source_.empty() || source_ == "0";
}

bool VideoSource::open() {
    if (isWebcamSource()) {
        std::cout << "[Sentinel] Opening webcam.\n";
        cap_.open(0);
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

bool VideoSource::read(cv::Mat& frame) {
    if (!cap_.isOpened()) {
        return false;
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