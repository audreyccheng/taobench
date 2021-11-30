#pragma once

#ifndef TIMER_H_
#define TIMER_H_

#include <chrono>
#include <string>

namespace benchmark {

namespace utils {

 // returns number of nanoseconds since epoch, as a string
std::string CurrentTimeNanos();

int64_t CurrentTimeNanos(int ignore);

template <typename R, typename P = std::ratio<1>>
class Timer {
 public:
  void Start() {
    time_ = Clock::now();
  }

  R End() {
    Duration span;
    Clock::time_point t = Clock::now();
    span = std::chrono::duration_cast<Duration>(t - time_);
    return span.count();
  }

  R GetStartTime() {
    Duration span = std::chrono::duration_cast<Duration>(time_.time_since_epoch());
    return span.count();
  }

 private:
  using Duration = std::chrono::duration<R, P>;
  using Clock = std::chrono::high_resolution_clock;

  Clock::time_point time_;
};

} // utils

} // benchmark

#endif // TIMER_H_
