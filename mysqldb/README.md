# MySQL

## Building TAOBench with MySQL
You will need a distribution of `cmake` and `g++-11` to build TAOBench. Additionally, the MySQL driver relies on

1. MySQL C API development files (libmysqlclient-dev) for MySQL version 5.7.
   Currently, our benchmark driver doesn't support the MySQL 8.0 protocol.
2. A [C++ wrapper](https://github.com/seznam/SuperiorMySqlpp) for the MySQL C API.

On Ubuntu 18.04:
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

## Configuring TAOBench with MySQL
First, create a database using `CREATE DATABASE <name>`, and access it with
`USE <name>`.

Then, create an `objects` and an `edges` table with the following schemas:
```sql
CREATE TABLE objects(
    id BIGINT PRIMARY KEY,
    timestamp BIGINT,
    value VARCHAR(150));
CREATE TABLE edges(
    id1 BIGINT,
    id2 BIGINT,
    type BIGINT,
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

## Creating a Cluster
Both TiDB and PlanetScale are MySQL compatible databases and have been
successfully used to run TAOBench.

### TiDB
Navigate to the [TiDB Cloud page](https://tidbcloud.com/console/clusters), and use the "Create Cluster" tool to create a cluster. Obtain connection details by following [these instructions](https://docs.pingcap.com/tidbcloud/connect-to-tidb-cluster).

### PlanetScale
Navigate to the [PlanetScale page](https://app.planetscale.com), and use the "New database" tool to create a database. Obtain connection details by following [these instructions](https://docs.planetscale.com/tutorials/connect-any-application).
