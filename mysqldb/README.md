# MySQL

## Building TAOBench with MySQL
You will need a distribution of `cmake` and `g++-11` to build TAOBench. Additionally, the MySQL driver relies on

1. MySQL C API development files (libmysqlclient-dev) for MySQL version 5.7
2. a [C++ wrapper](https://github.com/seznam/SuperiorMySqlpp) for the MySQL C API

On Ubuntu:
```
# TAOBench essentials
apt-get update
apt-get install -y software-properties-common
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get install -y build-essential cmake g++-11

# MySQL requirements
apt-get install -y libmysqlclient-dev
git clone https://github.com/seznam/SuperiorMySqlpp
cp -Rf SuperiorMySqlpp/include/* /usr/include
```

Modify the include directory for SuperiorMySqlpp in `CMakeLists.txt` to the download location, if necessary. Then build the benchmark.
```
cmake . -DWITH_MYSQL=ON
make
```

## Running TAOBench with MySQL
### Configuration
First, create a database using `CREATE DATABASE <name>`, and access it with
`USE <name>`.

Then, create an `objects` and an `edges` table with the following schemas:
```sql
DROP TABLE IF EXISTS objects;
CREATE TABLE objects(
    id BIGINT PRIMARY KEY,
    timestamp BIGINT,
    value VARCHAR(150));
DROP TABLE IF EXISTS edges;
CREATE TABLE edges(
    id1 BIGINT,
    id2 BIGINT,
    type VARCHAR(63),
    timestamp BIGINT,
    value VARCHAR(150),
    PRIMARY KEY CLUSTERED (id1, id2, type));
```

Configure your MySQL connection by filling the fields in `mysqldb/mysql_db.properties`:
```
mysqldb.dbname=
mysqldb.url=
mysqldb.username=
mysqldb.password=
mysqldb.dbport=
```

The benchmark runs in two phases. First, the `-load` phase is needed to populate the database with row entiries. Afterwards, the `-t` transaction phase is used to make queries against the database and benchmark performance. 

### Load Phase
Example load
```
./benchmark -db mysql -P mysqldb/mysql_db.properties -C src/workload_a.json -threads 200 -n 165000000 -load
```
`-threads` controls the number of threads used for loads. `-n` indicates roughly the number of rows to insert.

`KEY_POOL_FACTOR` is defined in `src/constants.h`. Additionally, `src/consants.h` defines `VALUE_SIZE_BYTES` which is the amount of data to insert into each row, and `src/workload_loader.cc` contains `WRITE_BATCH_SIZE` which defines the number of rows to batch together for each write during the load phase.

### Transaction phase
Example run
```
./benchmark -db mysql -P mysqldb/mysql_db.properties -C src/workload_a.json -E experiment_runs.txt -t -threads 32
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

## Creating a Cluster
Both TiDB and PlanetScale are MySQL compatible databases and have been
successfully used to run TAOBench.

### TiDB
Navigate to the [TiDB Cloud page](https://tidbcloud.com/console/clusters), and use the "Create Cluster" tool to create a cluster. Obtain connection details by following [these instructions](https://docs.pingcap.com/tidbcloud/connect-to-tidb-cluster).

### PlanetScale
Navigate to the [PlanetScale page](https://app.planetscale.com), and use the "New database" tool to create a database. Obtain connection details by following [these instructions](https://docs.planetscale.com/tutorials/connect-any-application).
