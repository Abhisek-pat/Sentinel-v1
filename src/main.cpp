#include "app/pipeline.h"

#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
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

    pipeline.run();

    std::cout << "[Sentinel] main() finished." << std::endl;
    return 0;
}
