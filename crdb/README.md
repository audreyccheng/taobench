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
	value varchar(150));
drop table if exists edges;
create table edges(
	id1 INT,
	id2 INT,
	type INT,
	timestamp bigint,
	value varchar(150)),
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
