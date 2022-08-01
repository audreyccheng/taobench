#pragma once
#include <limits>

namespace benchmark {
  namespace constants {
    // The number of logical shards. The keyspace is divided into NUM_SHARDS
    // equally spaced key ranges. The distribution of keys across these ranges is
    // determined by the workload configuration parameters.
    constexpr int NUM_SHARDS = 50;
    static_assert(NUM_SHARDS < 127, "Number of shards must be less than 127 for bit-packing support.");
    
    // Size for batch inserts. Each thread inserts WRITE_BATCH_SIZE keys per
    // database request.
    constexpr int WRITE_BATCH_SIZE = 256;
    // Size for batch reads. Each thread reads READ_BATCH_SIZE keys per
    // database request.
    constexpr int READ_BATCH_SIZE = 500;
    
    // TODO(jchan): This constant is currently unused. Its intended use is to
    // set the number of generated edges in the batch insert to be:
    //   KEY_POOL_FACTOR * -n * E[write_txn_size].
    constexpr int KEY_POOL_FACTOR = 3;

    // Defines the number of bytes stored in the value of each object/association.
    constexpr int VALUE_SIZE_BYTES = 150;

    // Initial backoff limit for a failed operation or transaction; grows exponentially.
    constexpr int INITIAL_BACKOFF_LIMIT_MICROS = 2000;

    // Maximum length for experiments, in seconds. Client threads will be
    // terminated when they have hit their num_ops or when this limit is exceeded,
    // whichever is first.
    constexpr double TIMEOUT_LIMIT_SECONDS = 60.0 * 10.2;
  }
}
