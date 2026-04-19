#include "ui/overlay_renderer.h"

#include <algorithm>

std::string OverlayRenderer::truncateText(const std::string& text, std::size_t max_len) {
    if (text.size() <= max_len) {
        return text;
    }
    if (max_len <= 3) {
        return text.substr(0, max_len);
    }
    return text.substr(0, max_len - 3) + "...";
}

void OverlayRenderer::drawDetections(cv::Mat& frame,
                                     const std::vector<Detection>& detections) const {
    for (const auto& det : detections) {
        cv::rectangle(frame, det.box, cv::Scalar(0, 255, 0), 2);

        std::string label;
        if (det.track_id >= 0) {
            label = "ID " + std::to_string(det.track_id) +
                    " | " + det.class_name +
                    " | " + cv::format("%.1fs", det.dwell_time_sec);
        } else {
            label = det.class_name;
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
    (void)source_label;

    const int panel_x = 10;
    const int panel_y = 10;
    const int panel_w = 250;
    const int panel_h = 85;

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(30, 30, 30),
                  cv::FILLED);

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(0, 255, 0),
                  1);

    int y = panel_y + 25;
    drawTextLine(frame, "Sentinel", panel_x + 10, y, 0.8, cv::Scalar(0, 255, 0), 2);
    y += 28;

    drawTextLine(frame, "FPS: " + cv::format("%.2f", fps), panel_x + 10, y, 0.6, cv::Scalar(255, 255, 255), 1);
    y += 24;

    drawTextLine(frame, "Frame: " + cv::format("%.2f ms", frame_time_ms), panel_x + 10, y, 0.6, cv::Scalar(255, 255, 255), 1);
}

void OverlayRenderer::drawEvents(cv::Mat& frame,
                                 const std::vector<std::string>& events) const {
    const int panel_w = 390;
    const int panel_h = 85;
    const int panel_x = frame.cols - panel_w - 10;
    const int panel_y = frame.rows - panel_h - 10;

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(30, 30, 30),
                  cv::FILLED);

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(0, 255, 255),
                  1);

    drawTextLine(frame, "Recent Events", panel_x + 10, panel_y + 20, 0.55, cv::Scalar(0, 255, 255), 1);

    int y = panel_y + 45;
    int count = 0;

    for (auto it = events.rbegin(); it != events.rend() && count < 2; ++it, ++count) {
        drawTextLine(frame,
                     truncateText(*it, 52),
                     panel_x + 10,
                     y,
                     0.45,
                     cv::Scalar(255, 255, 255),
                     1);
        y += 22;
    }
}

void OverlayRenderer::drawLlmOutput(cv::Mat& frame,
                                    const std::string& summary,
                                    const std::string& risk_level) const {
    const int panel_x = 10;
    const int panel_h = 95;
    const int panel_w = 430;
    const int panel_y = frame.rows - panel_h - 105;

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  cv::Scalar(30, 30, 30),
                  cv::FILLED);

    cv::Scalar risk_color(0, 255, 0);
    if (risk_level == "medium") {
        risk_color = cv::Scalar(0, 255, 255);
    } else if (risk_level == "high") {
        risk_color = cv::Scalar(0, 0, 255);
    }

    cv::rectangle(frame,
                  cv::Rect(panel_x, panel_y, panel_w, panel_h),
                  risk_color,
                  1);

    drawTextLine(frame,
                 "LLM Assessment",
                 panel_x + 10,
                 panel_y + 22,
                 0.55,
                 cv::Scalar(255, 255, 255),
                 1);

    drawTextLine(frame,
                 "Risk: " + risk_level,
                 panel_x + 10,
                 panel_y + 50,
                 0.75,
                 risk_color,
                 2);

    const std::string short_summary = truncateText(summary, 58);
    drawTextLine(frame,
                 short_summary,
                 panel_x + 10,
                 panel_y + 78,
                 0.48,
                 cv::Scalar(255, 255, 255),
                 1);
}

void OverlayRenderer::drawTextLine(cv::Mat& frame,
                                   const std::string& text,
                                   int x,
                                   int y,
                                   double scale,
                                   const cv::Scalar& color,
                                   int thickness) const {
    cv::putText(frame,
                text,
                cv::Point(x, y),
                cv::FONT_HERSHEY_SIMPLEX,
                scale,
                color,
                thickness,
                cv::LINE_AA);
}