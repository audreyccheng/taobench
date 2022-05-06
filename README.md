# TAOBench
A distributed database benchmark based on TAO's workload

## Building

Use `make` to build.

## Running

Load data with test DB:
```
./benchmark -threads 1 -db test -P /Users/hamzaqadeer/research/distributed-db-benchmark/mysqldb/mysql_db.properties  -C /Users/hamzaqadeer/research/distributed-db-benchmark/src/test.json -n 100000 > dump3.txt
```

# CRDB
## Setting up CRDB
Create a CockroachDB database.

Set up the following tables
```sql
drop table if exists objects;
create table objects(
	id varchar(63) primary key,
	timestamp bigint,
	value text);
drop table if exists edges;
create table edges(
	id1 varchar(63),
	id2 varchar(63),
	type varchar(63),
	timestamp bigint,
	value text,
	primary key (id1, id2, type));
```

## Installing dependencies
### Install postgres and postgres C libraries (libpq):
```
sudo apt-get install postgresql libpq-dev
```

### Install libpqxx: http://pqxx.org/development/libpqxx/
Confirm that your setup has CMake installed. If not, install CMake: https://cmake.org/

Clone the libpqxx repo
```
git clone https://github.com/jtv/libpqxx.git
```
Build the libpqxx library: https://github.com/jtv/libpqxx/blob/master/BUILDING-configure.md
```
./configure
make
sudo make install
```

## Configuration and Build
Copy the connection string for the CRDB database into `crdb/crdb.properties`. For example, using CockroachCloud:
```properties
crdb.connectionstring=postgresql://<username>:<password>@berkeley-benchmark-7q7.aws-us-west-2.cockroachlabs.cloud:26257/defaultdb?sslmode=verify-full&sslrootcert=/home/ubuntu/Library/CockroachCloud/certs/berkeley-benchmark-ca.crt
```

Use CMake to generate build files and build application using make. In the future, only running make is necessary unless changes to CMakeLists.txt are made.
```
cmake .
make
```

Run the benchmark with
```
./benchmark
```

## Running the Benchamark
The benchmark runs in two phases. First, the `-load` phase is needed to populate the database with row entiries. Afterwards, the `-t` transaction phase is used to make queries against the database and benchmark performance.

### Load Phase
Example load
```
./benchmark -threads 4 -db crdb -P crdb/crdb.properties -C src/test.json -threads 4 -n 10000000 -load
```
`-threads` controls the number of threads used for loads. `-n` indicates roughly the number of rows to insert, where the total number of rows to insert is `(-n value) \* KEY_POOL_FACTOR \* Expected transaction size`.

`KEY_POOL_FACTOR` is defined in `src/workload.cc`. Additionally, `src/workload.cc` defines `VALUE_SIZE_BYTES` which is the amount of data to insert into each row, and `src/workload_loader.cc` contains `WRITE_BATCH_SIZE` which defines the number of rows to batch together for each write during the load phase.

### Transaction phase
Example run
```
./benchmark -db crdb -P crdb/crdb.properties -C src/test.json -E experiment_runs.txt -t -threads 100 -rows 5400000
```
Transaction phases contain a bulk read at the beginning to read `-rows` rows into memory. `-threads` specifies how many threads to use for this bulk read phase, and `src/workload_loader.cc` defines `READ_BATCH_SIZE` to specify how many rows are batched together for each read.

Afterwards, the transaction phases runs a series of experiments against the database, which are defined in a text file inputted with `-E`. The file is of the format is shown below, where each line specifies a separate experiment
```
num_threads,total_num_operations,target_operations_per_second
```

Example experiment_runs.txt
```
1000,600000,1000
1000,1200000,2000
1000,1800000,3000
```

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

# TiDB and PlanetScale

The MySQL driver is compatible both TiDB and PlanetScale.

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

# Yugabyte
Here are the intructions to install yugabyteDB.

## Installing dependencies
### Install CMake, Postgres and Postgres C++ libraries (libpq):
```
sudo apt-get update
sudo apt-get install build-essential cmake libpq-dev postgresql
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
sudo apt install gcc-11 g++-11
```

### Install [libpqxx](http://pqxx.org/development/libpqxx):
Clone the libpqxx repo
```
git clone https://github.com/jtv/libpqxx.git
```
[Build](https://github.com/jtv/libpqxx/blob/master/BUILDING-configure.md) the libpqxx library:
```
./configure CXX=g++-11
make
sudo make install
```

## Setting up YSQL
Create a YSQL database.

### Connecting to YSQL
If you are using [YugabyteCloud](hhttps://cloud.yugabyte.com/login), connect to YSQL by either using command line (+ Yugabyte client installation) or connection string

### Setting the database schema
Connect to the client terminal on [YugabyteCloud](hhttps://cloud.yugabyte.com/login) and create a database called `test`:
```
yugabyte=# CREATE DATABASE test;
yugabyte=# \c test;
```
Create the following tables:
```sql
drop table if exists objects;
create table objects(
	id bigint,
	timestamp bigint,
	value text,
	primary key (id ASC));
drop table if exists edges;
create table edges(
	id1 bigint,
	id2 bigint,
	type smallint,
	timestamp bigint,
	value text,
	primary key (id1 ASC, id2 ASC, type ASC));
```

## Configuration and Build
Copy the connection string for the YSQL database into `ybsql_db/ybsql_db.properties`. For example, using YugabyteCloud:
```properties
ybsql_db.string=host=<host>.aws.ybdb.io port=5433 dbname=test user=admin password=<password>
```

Copy the CMakeLists_Yugabyte.txt over to the CMakeLists.txt
```
cp CMakeLists_Yugabyte.txt CMakeLists.txt
```

Use CMake to generate build files and build application using make. In the future, only running make is necessary unless changes to CMakeLists.txt are made.
```
cmake .
make
```

Run the benchmark with
```
./benchmark
```

## Running the Benchamark
The benchmark runs in two phases. First, the `-load` phase is needed to populate the database with row entiries. Afterwards, the `-t` transaction phase is used to make queries against the database and benchmark performance. 

### Load Phase
Example load
```
./benchmark -db ybsql -P ybsql/ybsql_db.properties -C src/workload_1.json -threads 10 -n 165000000 -load
```
`-threads` controls the number of threads used for loads. `-n` indicates roughly the number of rows to insert.

`KEY_POOL_FACTOR` is defined in `src/constants.h`. Additionally, `src/consants.h` defines `VALUE_SIZE_BYTES` which is the amount of data to insert into each row, and `src/workload_loader.cc` contains `WRITE_BATCH_SIZE` which defines the number of rows to batch together for each write during the load phase.

### Transaction phase
Example run
```
./benchmark -db ybsql -P ybsql/ybsql_db.properties -C src/workload_1.json -E experiment_runs.txt -t -threads 50
```
Transaction phases contain a bulk read at the beginning to read all the rows in the database into memory. `-threads` specifies how many threads to use for this bulk read phase, and `src/constants.h` defines `READ_BATCH_SIZE` to specify how many rows are batched together for each read. 

Afterwards, the transaction phases runs a series of experiments against the database, which are defined in a text file inputted with `-E`. The file is of the format is shown below, where each line specifies a separate experiment
```
num_threads,total_num_operations,target_operations_per_second
```

Example experiment_runs.txt
```
1000,600000,1000
1000,1200000,2000
1000,1800000,3000
```

### Recording Results
Latency information is outputed to a folder specified in `WriteLatencies()` inside `measurements.cc`. Changing the filename path can change where the latency information is written to.
