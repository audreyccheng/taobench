# Distributed DB Benchmark

This branch was used to run experiments on TiDB. However, because TiDB exposes a MySQL interface, these instructions should work for any ordinary MySQL database in addition to TiDB.

## Building

### Prerequisites:

1) MySQL client library. We used MySQL 5.7, though later versions should also work. You will need to add the path in `CMakeLists.txt`.
2) SuperiorMySqlpp header-only library. No installation required for this, just clone and modify in the include path in `CMakeLists.txt`.
3) `g++-11` compiler.

### Build

``
cmake .
make
``

## Running

Bulk load phase:

``
./benchmark -threads 2 -db mysql -P ./mysqldb/mysql_db.properties -C ./src/test_txn.json -load -n 165000000
``

Running experiments:

``
./benchmark -threads 1000 -db spanner -P ./spanner_db/spanner.properties -C ./src/test_txn.json -run -E ./experiments.txt
``

In the load phase, the `-threads` parameter specifies the number of threads used for bulk loads (recommended: 2-4). For runs, `-threads` instead specifies the maximum number of threads used by any experiment listed in `experiments.txt`. The number of threads used per experiment is configured in the `experiments.txt` file. Each line of that file should be of the format:

`num_threads,num_ops,target_throughput.`

## Info

Framework based on a fork of [YCSB-C](https://github.com/ls4154/YCSB-cpp).
