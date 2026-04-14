#pragma once

#include "detection/detector.h"

#include <opencv2/opencv.hpp>

#include <string>
#include <unordered_map>
#include <vector>

struct ZoneEvent {
    std::string message;
};

class ZoneManager {
public:
    ZoneManager();

    void initialize(int frame_width, int frame_height);

    std::vector<ZoneEvent> update(const std::vector<Detection>& detections,
                                  double current_time_sec);

    void drawZones(cv::Mat& frame) const;

    std::string getZoneForTrack(int track_id) const;
    bool isTrackLoitering(int track_id) const;

private:
    struct Zone {
        std::string name;
        cv::Rect area;
    };

    struct ZoneState {
        int track_id{-1};
        double enter_time{0.0};
        double last_seen_time{0.0};
        bool inside{false};
        bool loitering_triggered{false};
    };

private:
    std::vector<Zone> zones_;
    std::unordered_map<std::string, std::unordered_map<int, ZoneState>> zone_states_;
    double loiter_threshold_sec_{10.0};

private:
    bool isInside(const cv::Rect& box, const cv::Rect& zone) const;
};