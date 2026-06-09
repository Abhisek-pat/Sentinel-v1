#include "app/pipeline.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "[Sentinel] main() started." << std::endl;
    std::cout << "[Sentinel] Build variant: " << SENTINEL_VARIANT << std::endl;

    // Use the default webcam unless a file path or RTSP URL is provided.
    std::string source = "0";

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
