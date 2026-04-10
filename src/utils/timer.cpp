#include "utils/timer.h"

Timer::Timer()
    : start_time_(Clock::now()) {}

void Timer::start() {
    start_time_ = Clock::now();
}

double Timer::stopMilliseconds() const {
    return elapsedMilliseconds();
}

double Timer::elapsedMilliseconds() const {
    const auto end_time = Clock::now();
    const auto duration =
        std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end_time - start_time_);

    return duration.count();
}