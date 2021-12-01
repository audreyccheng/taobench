
# YugabyteDB 
Here are the intructions to install yugabyteDB.

## Installing yugabyteDB
1. For Linux, install wget and curl:
```
apt install wget
apt install curl
```
2. Download the YugabyteDB package using the following wget command:
```
wget https://downloads.yugabyte.com/releases/2.11.0.0/yugabyte-2.11.0.0-b7-linux-x86_64.tar.gz
```
3. Extract the package and then change directories to the YugabyteDB home:
```
tar xvfz yugabyte-2.11.0.0-b7-linux-x86_64.tar.gz && cd yugabyte-2.11.0.0/
```
4. configure YugabyteDB by running the following shell script:
```
./bin/post_install.sh
```

## Installing the libpqxx driver
1. Prerequisite:

    1. Installed YugabyteDB, and created a universe with YSQL enabled.
    2. Have a 32-bit (x86) or 64-bit (x64) architecture machine.
    3. Have gcc 4.1.2 or later, clang 3.4 or later installed.

2. Get the source:
```
git clone https://github.com/jtv/libpqxx.git
```

3. Make sure that the PostgreSQL bin directory is on the command path:
```
export PATH=$PATH:<yugabyte-install-dir>/postgres/bin
```

4. Run the following commands to build and install:
```
cd libpqxx
./configure
make
make install
```

5. Include the directory of `libpqxx` in `CMakeLists.txt`


Source: 
1. https://docs.yugabyte.com/latest/quick-start/install/linux/#configure-yugabytedb
2. https://docs.yugabyte.com/latest/quick-start/build-apps/cpp/ysql/#dependencies

## Creating a local cluster
1. Run the following command to create a single-node local cluster:
```
./bin/yugabyted start
```

## Creating a database
1. Run the following command inside `/yugabyte-2.9.0.0` to enter the yugabyte shell:
```
./bin/ysqlsh -h 127.0.0.1 -p 5433 -U yugabyte
```
2. Create a database called `test`:
```
yugabyte=# CREATE DATABASE test;
```
3. Connect to the new database using the YSQL shell `\c` meta command:
```
yugabyte=# \c test;
```

4. Run the schema code below to create two tables `objects` and `edges`:
```
test=> create table objects(id varchar(63),
                     timestamp bigint,
                     value text,
                     primary key(id HASH));

test=> create table edges(
       id1 varchar(63),
       id2 varchar(63),
       type varchar(63),
       timestamp bigint,
       value text,
 primary key (id1 HASH, id2 ASC, type ASC));
 ```

## Running Workload

1. run `make` to build
2. load the data
```
./benchmark -db ybsql -P ybsql_db/ybsql_db.properties -C src/test.json -load -n 10000000 -threads 100
```
3. run the data
```
./benchmark -db ybsql -P ybsql_db/ybsql_db.properties -C src/test.json -rows 2000000 -t -E ysql_workload1_exp/1k -threads 100 > w1.txt
```

