#pragma once

#include <opencv2/opencv.hpp>

#include <string>

class VideoSource {
public:
    explicit VideoSource(const std::string& source);

    bool open();
    bool read(cv::Mat& frame);
    bool isOpened() const;

    double fps() const;
    int width() const;
    int height() const;

private:
    bool isWebcamSource() const;

private:
    std::string source_;
    cv::VideoCapture cap_;
};