#include "timer.h"

 int64_t benchmark::utils::CurrentTimeNanos() {
   std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
   return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
 }