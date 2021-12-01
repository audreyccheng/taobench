#ifndef MEASUREMENTS_H_
#define MEASUREMENTS_H_

#include "db.h"
#include "workload.h"

#include <atomic>
#include <mutex>

namespace benchmark {

class Measurements {
 public:
  Measurements();
  void Report(Operation op, uint64_t latency);
  uint64_t GetCount(Operation op) {
    return count_[static_cast<int>(op)].load(std::memory_order_relaxed);
  }
  double GetLatency(Operation op) {
    uint64_t cnt = GetCount(op);
    return cnt > 0
        ? static_cast<double>(latency_sum_[static_cast<int>(op)].load(std::memory_order_relaxed)) / cnt
        : 0.0;
  }
  std::string GetStatusMsg();
  std::string WriteLatencies();
  void Reset();
  uint64_t GetTotalNumOps();
 private:
  std::atomic<uint32_t> count_[static_cast<int>(Operation::MAXOPTYPE)];
  std::atomic<uint64_t> latency_sum_[static_cast<int>(Operation::MAXOPTYPE)];
  std::atomic<uint64_t> latency_min_[static_cast<int>(Operation::MAXOPTYPE)];
  std::atomic<uint64_t> latency_max_[static_cast<int>(Operation::MAXOPTYPE)];
  std::mutex vector_lock;
  std::vector<uint64_t> latencies_[static_cast<int>(Operation::MAXOPTYPE)];
  std::map<int, std::string> mapOfOps = {
        {0,"Insert"},
        {1,"Read"},
        {2,"Update"},
        {3,"Scan"},
        {4,"ReadModiyWrite"},
        {5,"Delete"},
        {6,"ReadTxn"},
        {7,"WriteTxn"},
        {8,"Max"},
  };
};

} // benchmark

#endif // MEASUREMENTS
