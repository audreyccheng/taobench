# TAOBench
A distributed database benchmark based on Meta's social graph workload.

## Building & Schema Setup

Follow the instructions in each database directory for build instructions and Schema setup.

## Configuration

### `experiments.txt` file:

TAOBench supports running multiple experiments in a single run via a configurable `experiments.txt` file. Each line of that file specifies a different experiment and should be of the format:

``num_threads,num_ops,target_throughput``

`num_threads` specifies the number of threads concurrently making requests during the experiment; `num_ops` specifies the total number of DB operations that will be run, and `target_throughput` specifies a maximum target throughput, beyond which the benchmark will throttle by putting threads to sleep. As a guideline, `num_ops` and `target_throughput` can be set to very high numbers (~ 1 billion) and effectively ignored; experiments will automatically timeout, and throughput targets can instead be tweaked by the `num_threads`, which is the number of client threads making requests in parallel.

### `src/constants.h`

Other benchmark-level attributes can be tweaked in this file. In particular, different values of READ_BATCH_SIZE and WRITE_BATCH_SIZE might improve performance for batch inserts and batch reads.

## Prepping the Database

This phase populates the DB tables with an initial set of edges and objects. We batch insert data into the DB and batch read them into memory to be used when running experiments. To run the batch insert phase, use the following command:

```
./benchmark -threads <num_threads> -db <db> -P path/to/database_properties.properties -C path/to/config.json -load -n <num_edges>
```

Ideal values for `num_threads` and `num_edges` will vary by database and by use-case, but 50 and 165,000,000 should be good starting points, respectively.

## Running Experiments

```
./benchmark -threads <num_threads> -db <db> -P path/to/database_properties.properties -C path/to/config.json -run -E path/to/experiments.txt
```

This command first batch reads all the keys that were inserted in the batch insert phase and then begins to run experiments. Note that the batch read phase is only run for the first experiment and can take several hours depending on the number of keys in the DB.
Here, `num_threads` specifies the number of threads used *for batch reading, not for the experiments.* The value specified here must be less than or equal to the number of shards. 50 is the default value.
