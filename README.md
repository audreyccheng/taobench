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

## Installing yugabyteDB
1. For Linux, install wget and curl:
```
apt install wget
apt install curl
```
2. Download the YugabyteDB package using the following wget command:
```
wget https://downloads.yugabyte.com/releases/2.11.0.0/yugabyte-2.11.0.0-b7-linux-x86_64.tar.gz
```
3. Extract the package and then change directories to the YugabyteDB home:
```
tar xvfz yugabyte-2.11.0.0-b7-linux-x86_64.tar.gz && cd yugabyte-2.11.0.0/
```
4. configure YugabyteDB by running the following shell script:
```
./bin/post_install.sh
```

## Installing the libpqxx driver
1. Prerequisite:

    1. Installed YugabyteDB, and created a universe with YSQL enabled.
    2. Have a 32-bit (x86) or 64-bit (x64) architecture machine.
    3. Have gcc 4.1.2 or later, clang 3.4 or later installed.

2. Get the source:
```
git clone https://github.com/jtv/libpqxx.git
```

3. Make sure that the PostgreSQL bin directory is on the command path:
```
export PATH=$PATH:<yugabyte-install-dir>/postgres/bin
```

4. Run the following commands to build and install:
```
cd libpqxx
./configure
make
make install
```

5. Include the directory of `libpqxx` in `CMakeLists.txt`


Source:
1. https://docs.yugabyte.com/latest/quick-start/install/linux/#configure-yugabytedb
2. https://docs.yugabyte.com/latest/quick-start/build-apps/cpp/ysql/#dependencies

## Creating a local cluster
1. Run the following command to create a single-node local cluster:
```
./bin/yugabyted start
```

## Creating a database
1. Run the following command inside `/yugabyte-2.9.0.0` to enter the yugabyte shell:
```
./bin/ysqlsh -h 127.0.0.1 -p 5433 -U yugabyte
```
2. Create a database called `test`:
```
yugabyte=# CREATE DATABASE test;
```
3. Connect to the new database using the YSQL shell `\c` meta command:
```
yugabyte=# \c test;
```

4. Run the schema code below to create two tables `objects` and `edges`:
```
test=> create table objects(id varchar(63),
                     timestamp bigint,
                     value text,
                     primary key(id HASH));

test=> create table edges(
       id1 varchar(63),
       id2 varchar(63),
       type varchar(63),
       timestamp bigint,
       value text,
 primary key (id1 HASH, id2 ASC, type ASC));
 ```

## Running Workload

1. run `make` to build
2. load the data
```
./benchmark -db ybsql -P ybsql_db/ybsql_db.properties -C src/test.json -load -n 10000000 -threads 100
```
3. run the data
```
./benchmark -db ybsql -P ybsql_db/ybsql_db.properties -C src/test.json -rows 2000000 -t -E ysql_workload1_exp/1k -threads 100 > w1.txt
```
