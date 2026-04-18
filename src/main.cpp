#include "app/pipeline.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "[Sentinel] main() started." << std::endl;

    // Default source:
    // Replace CAMERA_USER, CAMERA_PASSWORD, and CAMERA_IP with your actual values.
    std::string source =
        "rtsp://kunmunTapoCam:LeezaSonali_07@192.168.1.225:554/stream2";

    // Command-line override
    if (argc > 1) {
        source = argv[1];
    }

    std::cout << "[Sentinel] Source argument: " << source << std::endl;

    Pipeline pipeline(source);

    if (!pipeline.initialize()) {
        std::cerr << "[Sentinel] Failed to initialize pipeline." << std::endl;
        return 1;
    }

    pipeline.run();

    std::cout << "[Sentinel] main() finished." << std::endl;
    return 0;
}