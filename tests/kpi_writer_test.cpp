#include "monitoring/kpi_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "sentinel-kpi-writer-test";
    std::filesystem::remove_all(directory);

    try {
        {
            KpiWriter writer(directory.string());
            writer.writeReasoningRequest(
                "loitering",
                "{\n  \"persons\": [{\"track_id\": 7, \"loitering\": true}],\n"
                "  \"recent_events\": []\n}\n");
        }

        std::ifstream input(directory / "reasoning_requests.jsonl");
        std::string first_line;
        std::string second_line;
        std::getline(input, first_line);
        std::getline(input, second_line);

        require(!first_line.empty(), "reasoning request was not written");
        require(second_line.empty(), "reasoning request was not a single JSONL record");
        require(
            first_line.find("\"trigger\":\"loitering\"") != std::string::npos,
            "reasoning trigger was not written");
        require(
            first_line.find("\"scene_state\":{") != std::string::npos,
            "scene state was not embedded as JSON");
    } catch (const std::exception& error) {
        std::cerr << "kpi_writer_test failed: " << error.what() << "\n";
        std::filesystem::remove_all(directory);
        return 1;
    }

    std::filesystem::remove_all(directory);
    return 0;
}
