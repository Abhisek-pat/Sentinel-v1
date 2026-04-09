#pragma once

#include <string>

class VideoSource;

class Pipeline {
public:
    explicit Pipeline(const std::string& source);

    bool initialize();
    void run();

private:
    std::string source_;
};