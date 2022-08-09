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
	value varchar(150),
	primary key (id ASC));
drop table if exists edges;
create table edges(
	id1 bigint,
	id2 bigint,
	type smallint,
	timestamp bigint,
	value varchar(150),
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
