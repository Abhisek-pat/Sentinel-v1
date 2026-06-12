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
        int outside_observations{0};
    };

private:
    std::vector<Zone> zones_;
    std::unordered_map<std::string, std::unordered_map<int, ZoneState>> zone_states_;
    std::unordered_map<std::string, std::unordered_map<int, int>> entry_observations_;
    double loiter_threshold_sec_{10.0};
    double exit_timeout_sec_{1.5};
    double min_exit_event_dwell_sec_{2.0};
    double boundary_margin_ratio_{0.08};
    int entry_confirmation_observations_{2};
    int exit_confirmation_observations_{3};

private:
    static bool centerInside(const cv::Rect& box, const cv::Rect& zone);
    cv::Rect entryArea(const cv::Rect& zone) const;
    cv::Rect stayArea(const cv::Rect& zone) const;
};
