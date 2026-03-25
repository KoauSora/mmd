#include "timer.hpp"

namespace core {

Timer::Timer() {
    reset();
}

void Timer::reset() {
    last_ = clock::now();
}

float Timer::tick() {
    const auto now = clock::now();
    const std::chrono::duration<float> delta = now - last_;
    last_ = now;
    return delta.count();
}

} // namespace core