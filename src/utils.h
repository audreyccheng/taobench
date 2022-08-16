#ifndef UTILS_H_
#define UTILS_H_

#include <algorithm>
#include <cstdint>
#include <exception>
#include <random>
#include <thread>
#include "unistd.h"
#include <sched.h>

namespace benchmark {

namespace utils {

const uint64_t kFNVOffsetBasis64 = 0xCBF29CE484222325ull;
const uint64_t kFNVPrime64 = 1099511628211ull;

class Exception : public std::exception {
 public:
  Exception(const std::string &message) : message_(message) { }
  const char* what() const noexcept {
    return message_.c_str();
  }
 private:
  std::string message_;
};

// from https://stackoverflow.com/questions/1407786/how-to-set-cpu-affinity-of-a-particular-pthread
inline int PinThisThreadToCpu(uint32_t cpu) {
//  if (cpu >= std::thread::hardware_concurrency()) {
//    return EINVAL;
//  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

inline uint64_t FNVHash64(uint64_t val) {
  uint64_t hash = kFNVOffsetBasis64;

  for (int i = 0; i < 8; i++) {
    uint64_t octet = val & 0x00ff;
    val = val >> 8;

    hash = hash ^ octet;
    hash = hash * kFNVPrime64;
  }
  return hash;
}

inline uint64_t Hash(uint64_t val) { return FNVHash64(val); }

inline uint32_t ThreadLocalRandomInt() {
  static thread_local std::random_device rd;
  static thread_local std::minstd_rand rn(rd());
  return rn();
}

inline double ThreadLocalRandomDouble(double min = 0.0, double max = 1.0) {
  static thread_local std::random_device rd;
  static thread_local std::minstd_rand rn(rd());
  static thread_local std::uniform_real_distribution<double> uniform(min, max);
  return uniform(rn);
}

///
/// Returns an ASCII code that can be printed to desplay
///
inline char RandomPrintChar() {
  return rand() % 94 + 33;
}

inline bool StrToBool(std::string str) {
  std::transform(str.begin(), str.end(), str.begin(), ::tolower);
  if (str == "true" || str == "1") {
    return true;
  } else if (str == "false" || str == "0") {
    return false;
  } else {
    throw Exception("Invalid bool string: " + str);
  }
}

inline std::string Trim(const std::string &str) {
  auto front = std::find_if_not(str.begin(), str.end(), [](int c){ return std::isspace(c); });
  return std::string(front, std::find_if_not(str.rbegin(), std::string::const_reverse_iterator(front),
      [](int c){ return std::isspace(c); }).base());
}

} // utils

} // benchmark

#endif // UTILS_H_
