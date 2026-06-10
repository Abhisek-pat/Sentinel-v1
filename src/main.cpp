#include "app/pipeline.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    // Keep service logs visible immediately when stdout is connected to journald.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "[Sentinel] main() started." << std::endl;
    std::cout << "[Sentinel] Build variant: " << SENTINEL_VARIANT << std::endl;

    const char* configured_source = std::getenv("SENTINEL_SOURCE");
    std::string source = configured_source != nullptr ? configured_source : "0";

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

    if (!pipeline.run()) {
        std::cerr << "[Sentinel] Pipeline stopped because of a startup failure." << std::endl;
        return 1;
    }

    std::cout << "[Sentinel] main() finished." << std::endl;
    return 0;
}
