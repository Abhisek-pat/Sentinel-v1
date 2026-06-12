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

bool contains(const std::vector<ZoneEvent>& events, const std::string& text) {
    for (const auto& event : events) {
        if (event.message.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
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

        require(manager.update({trackedPerson(20, 100)}, 0.0).empty(), "entry confirmed too early");
        require(
            contains(manager.update({trackedPerson(20, 100)}, 0.2), "entered LEFT_ZONE"),
            "confirmed left-zone entry was not emitted");

        for (int i = 0; i < 8; ++i) {
            const int boundary_x = i % 2 == 0 ? 135 : 145;
            require(
                manager.update({trackedPerson(boundary_x, 100)}, 0.4 + i * 0.2).empty(),
                "boundary jitter emitted a zone transition");
        }

        require(manager.update({trackedPerson(20, 100)}, 2.5).empty(), "stable visit emitted an event");
        require(manager.update({trackedPerson(300, 20)}, 2.8).empty(), "exit emitted too early");
        require(manager.update({trackedPerson(300, 20)}, 3.4).empty(), "exit emitted too early");
        require(
            contains(manager.update({trackedPerson(300, 20)}, 4.2), "exited LEFT_ZONE"),
            "meaningful zone exit was not emitted");

        ZoneManager transient_manager;
        transient_manager.initialize(640, 360);
        transient_manager.update({trackedPerson(20, 100)}, 0.0);
        transient_manager.update({trackedPerson(20, 100)}, 0.2);
        transient_manager.update({trackedPerson(300, 20)}, 0.4);
        transient_manager.update({trackedPerson(300, 20)}, 1.0);
        require(
            transient_manager.update({trackedPerson(300, 20)}, 2.0).empty(),
            "short zone visit emitted an exit");

        ZoneManager interrupted_entry_manager;
        interrupted_entry_manager.initialize(640, 360);
        interrupted_entry_manager.update({trackedPerson(20, 100)}, 0.0);
        interrupted_entry_manager.update({}, 0.2);
        require(
            interrupted_entry_manager.update({trackedPerson(20, 100)}, 0.4).empty(),
            "non-consecutive observations confirmed a zone entry");

        ZoneManager loiter_manager;
        loiter_manager.initialize(640, 360);
        loiter_manager.update({trackedPerson(220, 120)}, 0.0);
        loiter_manager.update({trackedPerson(220, 120)}, 0.2);
        require(
            contains(loiter_manager.update({trackedPerson(220, 120)}, 10.5), "loitering"),
            "genuine loitering event was not emitted");
    } catch (const std::exception& error) {
        std::cerr << "zone_manager_test failed: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
