#include "ui/overlay_renderer.h"

void OverlayRenderer::drawStats(cv::Mat& frame,
                                double fps,
                                double frame_time_ms,
                                const std::string& source_label) const {
    const int start_x = 10;
    int y = 25;
    const int line_gap = 30;

    drawTextLine(frame, "Sentinel - Day 3", start_x, y);
    y += line_gap;

    drawTextLine(frame, "Source: " + source_label, start_x, y);
    y += line_gap;

    drawTextLine(frame, "FPS: " + cv::format("%.2f", fps), start_x, y);
    y += line_gap;

    drawTextLine(frame, "Frame Time: " + cv::format("%.2f ms", frame_time_ms), start_x, y);
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