#pragma once

#include <chrono>

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;

    Timer();

    void start();
    double stopMilliseconds() const;
    double elapsedMilliseconds() const;

private:
    Clock::time_point start_time_;
};