# Spanner
## Building

Follow the CMake instructions on the Spanner quickstart guide (https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/quickstart) to install the Spanner client libraries using vcpkg. Then run:

```
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .
make
```

The Spanner `project-id`, `instance-id`, and `database` can be configured in the `spanner_db/spanner.properties` file.

## Running

Bulk load phase:

```
./benchmark -threads 50 -db spanner -P ./spanner_db/spanner.properties -C ./src/test_txn.json -load -n 165000000
```

Running experiments:

```
./benchmark -threads 100 -db spanner -P ./spanner_db/spanner.properties -C ./src/test_txn.json -run -E ./experiments.txt
```

Note that the -threads parameter specifies how many threads are used for the batch reads (to load the keypool from the bulk load phase.)
The number of threads used per experiment is configured in the `experiments.txt` file. Each line of that file should be of the format:

``num_threads,num_ops,target_throughput``.

`num_ops` and `target_throughput` can be set to very high numbers (~ 1 billion); experiments will automatically time out in a little over ten minutes, and throughput targets can instead be tweaked by the `num_threads`, which is the number of client threads making requests in parallel.
