#ifndef COUNTDOWNLATCH_H_
#define COUNTDOWNLATCH_H_

#include <mutex>
#include <condition_variable>

class CountDownLatch {
 public:
  CountDownLatch(int count) : count_(count) {}
  // void Await() {
  //   std::unique_lock<std::mutex> lock(mu_);
  //   if (count_ > 0) {
  //     cv_.wait(lock);
  //   }
  // }
  bool AwaitFor(long timeout_sec) {
    std::unique_lock<std::mutex> lock(mu_);
    if (count_ > 0) {
      return cv_.wait_for(lock, std::chrono::seconds(timeout_sec)) == std::cv_status::no_timeout;
    }
    return true;
  }
  void CountDown() {
    std::unique_lock<std::mutex> lock(mu_);
    if (--count_ <= 0) {
      cv_.notify_all();
    }
  }
 private:
  int count_;
  std::mutex mu_;
  std::condition_variable cv_;
};

#endif // COUNTDOWNLATCH_H_
