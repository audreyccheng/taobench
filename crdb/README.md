# CockroachDB (Postgres)

## Dependencies

CockroachDB requires several dependencies to be installed.

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
```shell
./configure CXX=g++-11
make
sudo make install
```

### Build TAOBench
```
cmake . -DWITH_CRDB=ON
make
```

## Setting up CRDB
Create a CockroachDB database. The instructions below pertain to Cockroach Cloud.

### Connecting to CRDB (Cockroach Cloud)
If you are using [CockroachCloud](https://cockroachlabs.cloud/), click the "Connect" button in the top right corner. If you are trying to connect through a command line interface, follow the instructions in the "Command Line" tab of the Connect modal (copied below)
- Install the CRDB Client using the CLI command listed in the first step of the instructions
- Install the security certificate for your cluster using the second step of the instructions
- Connect to your CRDB cluster using the CLI command listed in the third step of the instructions

If you are trying to set up your machine to connect to Cockroach Cloud through a program, follow the instructions for "Connection String" (copied below)
- Install the security certificate for your cluster using the first step of the instructions
- Note the CRDB connection string listed in the second step of the instructions. This will be used below in the "Configuration and Build" section of the README.

### Setting the database schema
Create the following tables:
```sql
create table objects(
	id INT primary key,
	timestamp bigint,
	value varchar(150));
create table edges(
	id1 INT,
	id2 INT,
	type INT,
	timestamp bigint,
	value varchar(150)),
	primary key (id1, id2, type));
```

### Configuration
Copy the connection string for the CRDB database into `crdb/crdb.properties`. For example, using CockroachCloud:
```properties
crdb.connectionstring=postgresql://<username>:<password>@berkeley-benchmark-7q7.aws-us-west-2.cockroachlabs.cloud:26257/defaultdb?sslmode=verify-full&sslrootcert=/home/ubuntu/Library/CockroachCloud/certs/berkeley-benchmark-ca.crt
```
