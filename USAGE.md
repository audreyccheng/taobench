# Usage
## Overview
The general steps for running TAOBench are:

1. **Schema setup**: create data tables.
2. **Configure benchmark parameters**: pick a workload, set experiment
   parameters, and specify connection details.
3. **Load data**: generate a baseline social graph that subsequent requests
   operate on.
4. **Run experiments**.
5. **Interpret results**.

The following sections describe these steps in detail.

## Step 1. Schema setup
For SQL databases, TAOBench uses an `objects` and an `edges` table to represent
TAO's graph data model.

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

Schemas for specific SQL dialects are in the respective docs.

## Step 2. Configure benchmark parameters

### Executable Flags
The `taobench` executable takes the following flags:

- `-load`: Run the batch insert phase of the workload.
- `-run`: Run the transactions phase of the workload.
- `-load-threads <n>`: Number of threads for batch inserts (load) or batch reads (run) (default: 1).
- `-db <dbname>`: Specify the name of the DB adapter layer to use (default: basic). Supported names are `crdb`, `mysql`, `spanner`, and `yugabytedb`.
- `-p <propertyfile>`: Load properties from the given file. Multiple files can be specified, and will be processed in the order specified.
- `-c <configfile>`: Load workload config from the given file.
- `-e <experimentfile>`: Each line gives number of threads, warmup length, and experiment length.
- `-property <name>=<value>`: Specify a property to be passed to the DB and workloads multiple properties can be specified, and override any values in the propertyfile.
- `-s`: Print status every 10 seconds (use status.interval prop to override).
- `-n`: Number of edges in key pool (default: 165 million) to batch insert.
- `-spin`: Spin on waits rather than sleeping.

### Experiments

TAOBench supports running multiple experiments in a single run via a
configurable `experiments.txt` file. Each line of that file specifies a
different experiment and should be of the format:
`num_threads,warmup_len,exp_len`.

Specifically,

- `num_threads` specifies the number of threads concurrently making requests
  during the experiment
- `warmup_len` specifies the length in seconds of the warmup period, which is
  the amount of time spent running the workload without taking measurements
- `exp_len` specifies the length in seconds of the experiment

<details>
  <summary>Example <code>experiments.txt</code></summary>

```
2,10,150
16,10,150
128,10,150
1024,10,150
```
</details>

## Step 3. Load data

Populate the DB tables with an initial set of edges and objects. We batch
insert data into the DB and batch read them into memory to be used when running
experiments. To run the batch insert phase, use the following command:

```
./taobench -load-threads <num_threads> -db <db> \
           -p path/to/database_properties.properties -c path/to/config.json \
           -load -n <num_edges>
```

Ideal values for `num_threads` and `num_edges` will vary by database and by
use-case, but 50 and 165,000,000 should be good starting points, respectively.

While the performance of this phase is not benchmarked, it is slow and can be
made faster by setting the write batch size property (`-property
write_batch_size=<size>`). This property sets how many rows will be inserted per
database request in this loading phase.

## Step 4. Run experiments

This phase runs the workload.
```
./taobench -load-threads <num_threads> -db <db> \
           -p path/to/database_properties.properties -c path/to/config.json \
           -run -e path/to/experiments.txt
```

This command first batch reads all the keys that were inserted in the batch
insert phase and then begins to run experiments. Note that the batch read phase
is only run for the _first experiment_ and can take several hours depending on
the number of keys in the DB. Here, `num_threads` specifies the number of
threads used *for batch reading, not for the experiments.* The value specified
here must be less than or equal to the number of shards. 50 is the default
value.

While the performance of batch reads is not benchmarked, it is slow and can be
made faster by setting the read batch size property (`-property
read_batch_size=<size>`). This property sets how many rows will be read per
database request.

## Step 5. Interpret results
Here's a sample result of an experiment run. These statistics are printed to
standard output at the end of each experiment run.

<details>
  <summary>Sample output</summary>

```
Total runtime (sec): 61.0204
Runtime excluding warmup (sec): 50.9823
Total completed operations excluding warmup: 5955
Throughput excluding warmup: 116.805
Number of overtime operations: 7615
Number of failed operations: 0
5955 operations; [INSERT: Count=216 Max=99399.29 Min=992.38 Avg=35662.55] [READ: Count=4126 Max=96849.38 Min=256.38 Avg=12637.73] [UPDATE: Count=1190 Max=186863.46 Min=918.42 Avg=40857.72] [READTRANSACTION: Count=393 Max=5861590.29 Min=1301.79 Avg=219441.40] [WRITETRANSACTION: Count=30 Max=588020.75 Min=4498.29 Avg=150933.08] [WRITE: Count=1406 Max=186863.46 Min=918.42 Avg=40059.60]
```
</details>

A few clarifications:

- For throughput, each read/write/read transaction/write transaction counts as a
  single completed operation.
- The last line describes operation latencies. The "Count" is the number of
  completed operations. The "Max", "Min", and "Avg" are latencies in
  microseconds.  The `WRITE` operation category is an aggregate of
  inserts/updates/deletes.
