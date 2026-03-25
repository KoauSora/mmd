#pragma once

#include <chrono>

namespace core {

class Timer {
public:
    Timer();

    void reset();
    float tick();

private:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point last_;
};

} // namespace core