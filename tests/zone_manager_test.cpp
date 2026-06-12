#include "zones/zone_manager.h"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

Detection trackedPerson(int x, int y) {
    Detection detection;
    detection.track_id = 3;
    detection.class_id = 0;
    detection.class_name = "person";
    detection.confidence = 0.9f;
    detection.box = cv::Rect(x, y, 40, 80);
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
        ZoneManager manager;
        manager.initialize(640, 360);

        require(
            manager.update({trackedPerson(20, 100)}, 0.0).size() == 1,
            "left-zone entry was not emitted");
        require(
            manager.update({trackedPerson(300, 20)}, 0.5).empty(),
            "zone exit emitted on boundary jitter");
        require(
            manager.update({trackedPerson(20, 100)}, 1.0).empty(),
            "zone re-entry emitted during debounce");
        require(
            manager.update({trackedPerson(300, 20)}, 1.2).empty(),
            "zone exit emitted before debounce timeout");
        require(
            manager.update({trackedPerson(300, 20)}, 2.6).size() == 1,
            "zone exit was not emitted after timeout");
    } catch (const std::exception& error) {
        std::cerr << "zone_manager_test failed: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
