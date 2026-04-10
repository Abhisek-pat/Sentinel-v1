#include "app/pipeline.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "[Sentinel] main() started.\n";

    std::string source = "0";
    if (argc > 1) {
        source = argv[1];
    }

    std::cout << "[Sentinel] Source argument: " << source << "\n";

    Pipeline pipeline(source);

    if (!pipeline.initialize()) {
        std::cerr << "[Sentinel] Failed to initialize pipeline.\n";
        return 1;
    }

    pipeline.run();

    std::cout << "[Sentinel] main() finished.\n";
    return 0;
}