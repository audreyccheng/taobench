# TAOBench
A distributed database benchmark based on Meta's social graph workload.

## Building & Schema Setup

Follow the instructions in each database directory for build instructions and Schema setup.

For SQL databases, TAOBench uses an `objects` and an `edges` table to represent
TAO's graph data model:
```sql
CREATE TABLE objects (
    id BIGINT PRIMARY KEY,
    timestamp BIGINT,
    value VARCHAR(150));
CREATE TABLE edges (
    id1 BIGINT,
    id2 BIGINT,
    type BIGINT,
    timestamp BIGINT,
    value VARCHAR(150),
    PRIMARY KEY CLUSTERED (id1, id2, type));
```

## Configuration

### `experiments.txt` file:

TAOBench supports running multiple experiments in a single run via a configurable `experiments.txt` file. Each line of that file specifies a different experiment and should be of the format:

``num_threads,warmup_len,exp_len``

Specifically,

- `num_threads` specifies the number of threads concurrently making requests
  during the experiment
- `warmup_len` specifies the length in seconds of the warmup period, which is
  the amount of time spent running the workload without taking measurements
- `exp_len` specifies the length in seconds of the experiment

## Prepping the Database

This phase populates the DB tables with an initial set of edges and objects. We batch insert data into the DB and batch read them into memory to be used when running experiments. To run the batch insert phase, use the following command:

```
./taobench -threads <num_threads> -db <db> -p path/to/database_properties.properties -c path/to/config.json -load -n <num_edges>
```

Ideal values for `num_threads` and `num_edges` will vary by database and by use-case, but 50 and 165,000,000 should be good starting points, respectively.

## Running Experiments

```
./taobench -threads <num_threads> -db <db> -p path/to/database_properties.properties -c path/to/config.json -run -E path/to/experiments.txt
```

This command first batch reads all the keys that were inserted in the batch insert phase and then begins to run experiments. Note that the batch read phase is only run for the first experiment and can take several hours depending on the number of keys in the DB.
Here, `num_threads` specifies the number of threads used *for batch reading, not for the experiments.* The value specified here must be less than or equal to the number of shards. 50 is the default value.

## Interpreting Results
Here's a sample result of an experiment run. These statistics are printed to
standard output---here's a sample:
```
Total runtime(sec): 612.332
Runtime excluding warmup (sec): 552.331
Total completed operations excluding warmup: 955070
Throughput excluding warmup: 1729.16
Number of overtime operations excluding warmup: 958438
Number of failed operations excluding warmup: 3378
862657 operations; [INSERT: Count=31525 Max=212570.51 Min=3928.44 Avg=7536.34] [READ: Count=606680 Max=212879.86 Min=1483.02 Avg=2546.12] [UPDATE: Count=167828 Max=394803.53 Min=3993.65 Avg=7885.27] [READTRANSACTION: Count=53338 Max=998148.18 Min=5130.46 Avg=41861.58] [WRITETRANSACTION: Count=3286 Max=240072.81 Min=10341.05 Avg=37818.67] [WRITE: Count=199353 Max=394803.53 Min=3928.44 Avg=7830.09]
```

A few clarifications:

- For throughput, each read/write/read transaction/write transaction counts as a
  single completed operation.
- The last line describes operation latencies in microseconds. The `WRITE`
  operation category is an aggregate of inserts/updates/deletes.
