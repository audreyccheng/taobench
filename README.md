# taobench
A distributed database benchmark based on TAO's workload


# YugabyteDB
Switch to branch `yugabyte` to proceed. 
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

Source: 
1. https://docs.yugabyte.com/latest/quick-start/install/linux/#configure-yugabytedb
2. https://docs.yugabyte.com/latest/quick-start/build-apps/cpp/ysql/#dependencies

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


