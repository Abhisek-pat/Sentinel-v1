#include "app/pipeline.h"
#include "capture/video_source.h"
#include <opencv2/opencv.hpp>
#include <iostream>

Pipeline::Pipeline(const std::string& source)
    : source_(source) {}

bool Pipeline::initialize() {
    std::cout << "[Sentinel] Pipeline initialized.\n";
    return true;
}

void Pipeline::run() {
    VideoSource video_source(source_);

    if (!video_source.open()) {
        std::cerr << "[Sentinel] Could not open video source.\n";
        return;
    }

    cv::Mat frame;
    const std::string window_name = "Sentinel - Day 2 Video Input";

    while (true) {
        if (!video_source.read(frame)) {
            std::cout << "[Sentinel] End of stream or failed frame read.\n";
            break;
        }

        cv::imshow(window_name, frame);

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "[Sentinel] Exit requested by user.\n";
            break;
        }
    }

    cv::destroyAllWindows();
}