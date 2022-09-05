# PlanetScale / TiDB (MySQL)

## Dependencies
The MySQL driver relies on

1. MySQL C API development files (libmysqlclient-dev) for MySQL version 5.7.
   Currently, our benchmark driver doesn't support the MySQL 8.0 protocol.
2. A [C++ wrapper](https://github.com/seznam/SuperiorMySqlpp) for the MySQL C API.

<details>
<summary>Example: Ubuntu 18.04</summary>

```shell
# MySQL C API
apt-get install -y libmysqlclient-dev

# C++ wrapper
git clone https://github.com/seznam/SuperiorMySqlpp
cp -Rf SuperiorMySqlpp/include/* /usr/include
```
</details>

Modify the include directory for SuperiorMySqlpp in `CMakeLists.txt` to the
download location, if necessary. Then build the benchmark.
```shell
cmake . -DWITH_MYSQL=ON
make
```

## Setting up MySQL
Create an `objects` and an `edges` table with the following schemas:
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

Configure your MySQL connection by filling the fields in `mysqldb/mysql_db.properties`.

<details>
<summary>Properties</summary>

```
mysqldb.dbname=
mysqldb.url=
mysqldb.username=
mysqldb.password=
mysqldb.dbport=
```
</details>

## Creating a Cluster
TiDB and PlanetScale are MySQL-compatible databases, and TAOBench has been
run on both.

### TiDB
Navigate to the [TiDB Cloud page](https://tidbcloud.com/console/clusters), and use the "Create Cluster" tool to create a cluster. Obtain connection details by following [these instructions](https://docs.pingcap.com/tidbcloud/connect-to-tidb-cluster).

### PlanetScale
Navigate to the [PlanetScale page](https://app.planetscale.com), and use the "New database" tool to create a database. Obtain connection details by following [these instructions](https://docs.planetscale.com/tutorials/connect-any-application).
