# Running CRDB

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

## Setting up CRDB
Create a CockroachDB database.

### Connecting to CRDB (Cockroach Cloud)
If you are using [CockroachCloud](https://cockroachlabs.cloud/), click the "Connect" button in the top right corner. If you are trying to connect through a command line interface, follow the instructions in the "Command Line" tab of the Connect modal (copied below)
- Install the CRDB Client using the CLI command listed in the first step of the instructions
- Install the security certificate for your cluster using the second step of the instructions
- Connect to your CRDB cluster using the CLI command listed in the third step of the instructions

If you are trying to set up your machine to connect to COckroach Cloud through a program, follow the instructions for "Connection String" (copied below)
- Install the security certificate for your cluster using the first step of the instructions
- Note the CRDB connection string listed in the second step of the instructions. This will be used below in the "Configuration and Build" section of the README.

### Setting the database schema
Create the following tables:
```sql
drop table if exists objects;
create table objects(
	id INT primary key,
	timestamp bigint,
	value text);
drop table if exists edges;
create table edges(
	id1 INT,
	id2 INT,
	type INT,
	timestamp bigint,
	value text,
	primary key (id1, id2, type));
```

## Configuration and Build
Copy the connection string for the CRDB database into `crdb/crdb.properties`. For example, using CockroachCloud:
```properties
crdb.connectionstring=postgresql://<username>:<password>@berkeley-benchmark-7q7.aws-us-west-2.cockroachlabs.cloud:26257/defaultdb?sslmode=verify-full&sslrootcert=/home/ubuntu/Library/CockroachCloud/certs/berkeley-benchmark-ca.crt
```

Use CMake to generate build files and build application using make. In the future, only running make is necessary unless changes to CMakeLists.txt are made.
```
cmake . -DWITH_CRDB=ON
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
./benchmark -db crdb -P crdb/crdb.properties -C src/workload_a.json -threads 200 -n 165000000 -load
```
`-threads` controls the number of threads used for loads. `-n` indicates roughly the number of rows to insert.

`KEY_POOL_FACTOR` is defined in `src/constants.h`. Additionally, `src/consants.h` defines `VALUE_SIZE_BYTES` which is the amount of data to insert into each row, and `src/workload_loader.cc` contains `WRITE_BATCH_SIZE` which defines the number of rows to batch together for each write during the load phase.

### Transaction phase
Example run
```
./benchmark -db crdb -P crdb/crdb.properties -C src/workload_a.json -E experiment_runs.txt -t -threads 32
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
