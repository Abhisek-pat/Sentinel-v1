#include "app/pipeline.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string source = "0";

    if (argc > 1) {
        source = argv[1];
    }

    Pipeline pipeline(source);

    if (!pipeline.initialize()) {
        std::cerr << "[Sentinel] Failed to initialize pipeline.\n";
        return 1;
    }

    pipeline.run();
    return 0;
}