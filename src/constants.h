#pragma once

namespace benchmark {
  namespace constants {
    const int NUM_LOAD_SPREADERS = 100; // check workload.cc for explanation of load spreaders
    const int WRITE_BATCH_SIZE = 256; // batch size for inserts
    const int READ_BATCH_SIZE = 5000; 
    
    // number of generated edges in batch insert is KEY_POOL_FACTOR * -n * E[write_txn_size]
    const int KEY_POOL_FACTOR = 3;
    const int VALUE_SIZE_BYTES = 150;
  }
}