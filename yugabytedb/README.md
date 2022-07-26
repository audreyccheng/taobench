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
Copy the connection string for the YSQL database into `yugabytedb/yugabytedb.properties`. For example, using YugabyteCloud:
```properties
yugabytedb.string=host=<host>.aws.ybdb.io port=5433 dbname=test user=admin password=<password>
```

Use CMake to generate build files and build application using make. In the future, only running make is necessary unless changes to CMakeLists.txt are made.
```
cmake . -DWITH_YUGABYTE=ON
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
./benchmark -db yugabytedb -P yugabytedb/yugabytedb.properties -C src/workload_a.json -threads 10 -n 165000000 -load
```
`-threads` controls the number of threads used for loads. `-n` indicates roughly the number of rows to insert.

`KEY_POOL_FACTOR` is defined in `src/constants.h`. Additionally, `src/consants.h` defines `VALUE_SIZE_BYTES` which is the amount of data to insert into each row, and `src/workload_loader.cc` contains `WRITE_BATCH_SIZE` which defines the number of rows to batch together for each write during the load phase.

### Transaction phase
Example run
```
./benchmark -db yugabytedb -P yugabytedb/yugabytedb.properties -C src/workload_a.json -E experiment_runs.txt -t -threads 50
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
