# Running TAOBench on CRDB
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




