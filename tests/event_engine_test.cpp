#include "events/event_engine.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

Detection trackedPerson(int track_id) {
    Detection detection;
    detection.track_id = track_id;
    detection.class_id = 0;
    detection.class_name = "person";
    detection.confidence = 0.9f;
    detection.box = cv::Rect(20, 20, 100, 200);
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
        EventEngine engine;
        std::vector<Detection> visible{trackedPerson(7)};
        std::vector<Detection> empty;

        require(engine.update(visible, 0.0).empty(), "entry emitted immediately");
        require(engine.update(visible, 1.1).size() == 1, "entry was not emitted");
        require(engine.update(empty, 3.0).empty(), "exit emitted before timeout");
        require(engine.update(visible, 3.5).empty(), "recovered track emitted duplicate entry");
        require(engine.update(empty, 8.0).size() == 1, "exit was not emitted");
        require(engine.update(empty, 9.0).empty(), "exit was emitted repeatedly");
    } catch (const std::exception& error) {
        std::cerr << "event_engine_test failed: " << error.what() << "\n";
        return 1;
    }

    return 0;
}
