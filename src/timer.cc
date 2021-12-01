#include "timer.h"

 std::string benchmark::utils::CurrentTimeNanos() {
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  long timestamp_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
  return std::to_string(timestamp_nanos);
 }

 int64_t benchmark::utils::CurrentTimeNanos(int ignore) {
   std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
 }