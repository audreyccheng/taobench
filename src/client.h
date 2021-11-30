#ifndef CLIENT_H_
#define CLIENT_H_

#include <string>
#include <chrono>
#include <thread>
#include "db.h"
#include "workload.h"
#include "utils.h"
#include "countdown_latch.h"

namespace benchmark {

struct ClientThreadInfo {
  int completed_ops;
  int overtime_ops;
};

inline ClientThreadInfo ClientThread(benchmark::DB *db, benchmark::Workload *wl,
                        const int num_ops, const double target_throughput, bool init_wl,
                        bool init_db, bool cleanup_db, bool sleep_on_wait,
                        CountDownLatch *latch) {

  using namespace std::chrono;
  time_point<system_clock> start = system_clock::now();
  int64_t nanos_per_op = (int64_t) (1e9 / target_throughput);
  assert(nanos_per_op > 0);
  utils::Timer<int64_t, std::nano> timer;
  
  // random offset for each thread so that the DB isn't hit by all threads at once
  std::this_thread::sleep_for(std::chrono::nanoseconds(5000 + std::rand() % nanos_per_op));
                          
  int oks = 0;
  int overtime_ops = 0;
  for (int i = 0; i < num_ops; ++i) {
    timer.Start();
    oks += wl->DoRequest(*db);
    int64_t time_left = nanos_per_op - timer.End();
    time_point<system_clock> now = system_clock::now();
    duration<double> elapsed_time = now - start;
    if (elapsed_time.count() > 60.0 * 10.2) {
      break;
    }
    if (time_left < 0) {
      overtime_ops++; // we're failing to meet our throughput target
    } else if (sleep_on_wait) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(time_left));
    } else { // keep looping until wait is over
      while (utils::CurrentTimeNanos(0) < timer.GetStartTime() + time_left);
    }
  }

  if (cleanup_db) {
    db->Cleanup();
  }

  latch->CountDown();
  return {oks, overtime_ops};
}

} // benchmark

#endif // CLIENT_H_
