#include "app/pipeline.h"

#include "capture/video_source.h"
#include "ui/overlay_renderer.h"
#include "utils/timer.h"

#include <opencv2/opencv.hpp>

#include <iostream>
#include <string>

Pipeline::Pipeline(const std::string& source)
    : source_(source) {}

bool Pipeline::initialize() {
    std::cout << "[Sentinel] Pipeline initialized.\n";
    return true;
}

void Pipeline::run() {
    std::cout << "[Sentinel] Entering run().\n";

    VideoSource video_source(source_);

    if (!video_source.open()) {
        std::cerr << "[Sentinel] Could not open video source.\n";
        return;
    }

    OverlayRenderer overlay_renderer;
    cv::Mat frame;
    const std::string window_name = "Sentinel";
    const std::string source_label = (source_.empty() || source_ == "0") ? "webcam" : source_;

    double fps = 0.0;

    std::cout << "[Sentinel] Starting frame loop.\n";

    while (true) {
        Timer frame_timer;
        frame_timer.start();

        if (!video_source.read(frame)) {
            std::cout << "[Sentinel] End of stream or failed frame read.\n";
            break;
        }

        const double frame_time_ms = frame_timer.elapsedMilliseconds();

        if (frame_time_ms > 0.0) {
            const double instant_fps = 1000.0 / frame_time_ms;
            if (fps == 0.0) {
                fps = instant_fps;
            } else {
                fps = 0.9 * fps + 0.1 * instant_fps;
            }
        }

        overlay_renderer.drawStats(frame, fps, frame_time_ms, source_label);

        cv::imshow(window_name, frame);

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            std::cout << "[Sentinel] Exit requested by user.\n";
            break;
        }
    }

    cv::destroyAllWindows();
    std::cout << "[Sentinel] Exiting run().\n";
}