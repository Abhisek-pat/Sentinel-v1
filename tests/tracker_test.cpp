#include "tracking/tracker.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

Detection personAt(int x, int y) {
    Detection detection;
    detection.class_id = 0;
    detection.class_name = "person";
    detection.confidence = 0.9f;
    detection.box = cv::Rect(x, y, 100, 200);
    return detection;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        Tracker tracker;

        auto tracked = tracker.update({personAt(20, 20)});
        require(tracked.empty(), "transient detection was emitted");
        require(tracker.update({personAt(22, 20)}).empty(), "track confirmed too early");
        tracked = tracker.update({personAt(24, 20)});
        require(tracked.size() == 1, "expected confirmed track");
        const int initial_id = tracked[0].track_id;

        for (int i = 0; i < 10; ++i) {
            require(tracker.update({}).empty(), "missing track should not be emitted");
        }

        tracked = tracker.update({personAt(26, 20)});
        require(tracked.size() == 1, "expected recovered track");
        require(tracked[0].track_id == initial_id, "temporary miss changed track id");

        for (int i = 0; i < 16; ++i) {
            tracker.update({});
        }

        tracked = tracker.update({personAt(24, 20)});
        require(tracked.empty(), "replacement track confirmed immediately");
        tracker.update({personAt(25, 20)});
        tracked = tracker.update({personAt(26, 20)});
        require(tracked.size() == 1, "expected confirmed replacement track");
        require(tracked[0].track_id != initial_id, "expired track id was reused");
    } catch (const std::exception& error) {
        std::cerr << "tracker_test failed: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
