#pragma once

#include <opencv2/opencv.hpp>

#include <string>

class OverlayRenderer {
public:
    void drawStats(cv::Mat& frame,
                   double fps,
                   double frame_time_ms,
                   const std::string& source_label) const;

private:
    void drawTextLine(cv::Mat& frame,
                      const std::string& text,
                      int x,
                      int y) const;
};