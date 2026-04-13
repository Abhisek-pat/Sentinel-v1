#include "ui/overlay_renderer.h"

#include <algorithm>

void OverlayRenderer::drawDetections(cv::Mat& frame,
                                     const std::vector<Detection>& detections) const {
    for (const auto& det : detections) {
        cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);

        std::string label = det.class_name + " " + cv::format("%.2f", det.confidence);
        if (det.track_id >= 0) {
            label = "ID " + std::to_string(det.track_id) +
                    " | " + det.class_name +
                    " " + cv::format("%.2f", det.confidence) +
                    " | " + cv::format("%.1fs", det.dwell_time_sec);
        }

        int baseline = 0;
        const cv::Size text_size = cv::getTextSize(
            label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        const int text_x = det.box.x;
        const int text_y = std::max(20, det.box.y - 8);

        cv::rectangle(
            frame,
            cv::Rect(text_x,
                     text_y - text_size.height - 6,
                     text_size.width + 8,
                     text_size.height + 8),
            cv::Scalar(0, 255, 0),
            cv::FILLED);

        cv::putText(frame,
                    label,
                    cv::Point(text_x + 4, text_y - 4),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 0, 0),
                    1,
                    cv::LINE_AA);
    }
}

void OverlayRenderer::drawStats(cv::Mat& frame,
                                double fps,
                                double frame_time_ms,
                                const std::string& source_label) const {
    const int start_x = 10;
    int y = 25;
    const int line_gap = 30;

    drawTextLine(frame, "Sentinel - Day 9", start_x, y);
    y += line_gap;

    drawTextLine(frame, "Source: " + source_label, start_x, y);
    y += line_gap;

    drawTextLine(frame, "FPS: " + cv::format("%.2f", fps), start_x, y);
    y += line_gap;

    drawTextLine(frame, "Frame Time: " + cv::format("%.2f ms", frame_time_ms), start_x, y);
}

void OverlayRenderer::drawEvents(cv::Mat& frame,
                                 const std::vector<std::string>& events) const {
    int y = frame.rows - 20;
    const int max_events_to_draw = 3;

    int count = 0;
    for (auto it = events.rbegin(); it != events.rend() && count < max_events_to_draw; ++it, ++count) {
        cv::putText(frame,
                    *it,
                    cv::Point(10, y),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 255, 255),
                    1,
                    cv::LINE_AA);
        y -= 25;
    }
}

void OverlayRenderer::drawTextLine(cv::Mat& frame,
                                   const std::string& text,
                                   int x,
                                   int y) const {
    cv::putText(frame,
                text,
                cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX,
                0.7,
                cv::Scalar(0, 255, 0),
                2,
                cv::LINE_AA);
}