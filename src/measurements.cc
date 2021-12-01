#include "measurements.h"

#include <atomic>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace benchmark {

const char *kOperationString[static_cast<int>(Operation::MAXOPTYPE)] {
  "INSERT",
  "READ",
  "UPDATE",
  "SCAN",
  "READMODIFYWRITE",
  "DELETE",
  "READTRANSACTION",
  "WRITETRANSACTION"
};

Measurements::Measurements() : count_{}, latency_sum_{}, latency_max_{} {
  std::fill(std::begin(latency_min_), std::end(latency_min_), std::numeric_limits<uint64_t>::max());
  for (int i = 0; i < static_cast<int>(Operation::MAXOPTYPE); ++i) {
    latencies_[i].reserve(31000000);
  }
}

void Measurements::Report(Operation op, uint64_t latency) {
  count_[static_cast<int>(op)].fetch_add(1, std::memory_order_relaxed);
  latency_sum_[static_cast<int>(op)].fetch_add(latency, std::memory_order_relaxed);
  uint64_t prev_min = latency_min_[static_cast<int>(op)].load(std::memory_order_relaxed);
  while (prev_min > latency
         && !latency_min_[static_cast<int>(op)].compare_exchange_weak(prev_min, latency, std::memory_order_relaxed));
  uint64_t prev_max = latency_max_[static_cast<int>(op)].load(std::memory_order_relaxed);
  while (prev_max < latency
         && !latency_max_[static_cast<int>(op)].compare_exchange_weak(prev_max, latency, std::memory_order_relaxed));
  vector_lock.lock();
  latencies_[static_cast<int>(op)].emplace_back(latency);
  vector_lock.unlock();
}

std::string Measurements::GetStatusMsg() {
  std::ostringstream msg_stream;
  msg_stream.precision(2);
  uint64_t total_cnt = 0;
  msg_stream << std::fixed << " operations;";
  uint64_t write_cnt = 0;
  double write_total_latency = 0, write_min_latency = std::numeric_limits<double>::max(), write_max_latency = 0;
  for (int i = 0; i < static_cast<int>(Operation::MAXOPTYPE); i++) {
    Operation op = static_cast<Operation>(i);
    uint64_t cnt = GetCount(op);
    if (cnt == 0) {
      continue;
    }
    double op_max_latency = static_cast<double>(latency_max_[static_cast<int>(op)].load(std::memory_order_relaxed)) / 1000.0;
    double op_min_latency = static_cast<double>(latency_min_[static_cast<int>(op)].load(std::memory_order_relaxed)) / 1000.0;
    double op_sum_latency = static_cast<double>(latency_sum_[static_cast<int>(op)].load(std::memory_order_relaxed));
    msg_stream << " [" << kOperationString[static_cast<int>(op)] << ":"
               << " Count=" << cnt
               << " Max=" << op_max_latency
               << " Min=" << op_min_latency
               << " Avg="
               << ((cnt > 0)
                   ? op_sum_latency / cnt
                   : 0) / 1000.0
               << "]";
    total_cnt += cnt;
    if (op == Operation::UPDATE || op == Operation::INSERT || op == Operation::DELETE) {
      write_cnt += cnt;
      write_total_latency += op_sum_latency;
      write_max_latency = std::max(write_max_latency, op_max_latency);
      write_min_latency = std::min(write_min_latency, op_min_latency);
    }
  }
  msg_stream << " [" << "WRITE" << ":"
               << " Count=" << write_cnt
               << " Max=" << write_max_latency
               << " Min=" << write_min_latency
               << " Avg="
               << ((write_cnt > 0)
                   ? write_total_latency / write_cnt
                   : 0) / 1000.0
               << "]";
  return std::to_string(total_cnt) + msg_stream.str();
}

std::string Measurements::WriteLatencies() {
  auto now = std::chrono::system_clock::now();
  auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  auto value = now_ms.time_since_epoch();
  long curr_time = value.count();
  // write latencies of each type to a separate file
  for (int i = 0; i < static_cast<int>(Operation::MAXOPTYPE); ++i) {
    std::string filename= "final_results4/" + mapOfOps[i] + "_" + std::to_string(curr_time) + ".txt";
    std::ofstream outFile(filename);
    for (const auto &e : latencies_[i]) outFile << e << "\n";
  }
  return "Latencies written to [operation]_" + std::to_string(curr_time) + ".txt";
}

void Measurements::Reset() {
  std::fill(std::begin(count_), std::end(count_), 0);
  std::fill(std::begin(latency_sum_), std::end(latency_sum_), 0);
  std::fill(std::begin(latency_min_), std::end(latency_min_), std::numeric_limits<uint64_t>::max());
  std::fill(std::begin(latency_max_), std::end(latency_max_), 0);
  vector_lock.lock();
  for (int i = 0; i < static_cast<int>(Operation::MAXOPTYPE); ++i) {
    latencies_[i].clear();
  }
  vector_lock.unlock();
}

uint64_t Measurements::GetTotalNumOps() {
  uint64_t total_count = 0;
  for (int i = 0; i < static_cast<int>(Operation::MAXOPTYPE); ++i) {
    Operation op = static_cast<Operation>(i);
    total_count += GetCount(op);
  }
  return total_count;
}

} // benchmark
