#pragma once
#include <limits>

namespace benchmark {
  namespace constants {
    constexpr int NUM_SHARDS = 50;
    static_assert(NUM_SHARDS < 127, "Number of shards must be less than 127 for bit-packing support.");
    
    constexpr int WRITE_BATCH_SIZE = 256; // batch size for inserts
    constexpr int READ_BATCH_SIZE = 500; 
    
    // number of generated edges in batch insert is KEY_POOL_FACTOR * -n * E[write_txn_size]
    constexpr int KEY_POOL_FACTOR = 3;
    constexpr int VALUE_SIZE_BYTES = 150;

    // initial backoff limit for a failed operation or transaction; grows exponentially
    constexpr int INITIAL_BACKOFF_LIMIT_MICROS = 2000;

    // maximum length for experiments, in seconds
    constexpr double TIMEOUT_LIMIT_SECONDS = 60.0 * 10.2;
  }
}