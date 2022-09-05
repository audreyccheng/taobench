# Yugabyte (Postgres)
## Dependencies

YugabyteDB requires several dependencies to be installed. Note that the YugabyteDB driver for TAOBench supports YSQL currently.

### Install Postgres Libraries
```shell
apt-get install libpq-dev postgresql
```

### Install [libpqxx](http://pqxx.org/development/libpqxx)
Clone the libpqxx repo
```shell
git clone https://github.com/jtv/libpqxx.git
```
[Build](https://github.com/jtv/libpqxx/blob/master/BUILDING-configure.md) the libpqxx library:
```
./configure CXX=g++-11
make
sudo make install
```

### Build TAOBench
```
cmake . -DWITH_YUGABYTE=ON
make
```

## Setting up YSQL
Create a YSQL database. The instructions below pertain to YugabyteCloud.

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
create table objects(
	id bigint,
	timestamp bigint,
	value varchar(150),
	primary key (id ASC));
create table edges(
	id1 bigint,
	id2 bigint,
	type smallint,
	timestamp bigint,
	value varchar(150),
	primary key (id1 ASC, id2 ASC, type ASC));
```

### Configuration
Copy the connection string for the YSQL database into `yugabytedb/yugabytedb.properties`. For example, using YugabyteCloud:
```properties
yugabytedb.string=host=<host>.aws.ybdb.io port=5433 dbname=test user=admin password=<password>
```
