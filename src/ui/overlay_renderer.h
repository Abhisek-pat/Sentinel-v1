#pragma once

#include "detection/detector.h"

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

class OverlayRenderer {
public:
    void drawDetections(cv::Mat& frame,
                        const std::vector<Detection>& detections) const;

    void drawStats(cv::Mat& frame,
                   double fps,
                   double frame_time_ms,
                   const std::string& source_label) const;

    void drawEvents(cv::Mat& frame,
                    const std::vector<std::string>& events) const;

    void drawLlmOutput(cv::Mat& frame,
                       const std::string& summary,
                       const std::string& risk_level) const;

private:
    void drawTextLine(cv::Mat& frame,
                      const std::string& text,
                      int x,
                      int y,
                      double scale = 0.7,
                      const cv::Scalar& color = cv::Scalar(0, 255, 0),
                      int thickness = 2) const;

    static std::string truncateText(const std::string& text, std::size_t max_len);
};