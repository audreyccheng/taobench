# TAOBench
A distributed database benchmark based on Meta's social graph workload.

## Installation
### Dependencies
To build TAOBench, install:

- a C++17 compiler
- CMake (version 3.7 or higher)

<details>
<summary>Example: Ubuntu 18.04</summary>

```shell
apt-get update
apt-get install -y software-properties-common
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get install -y build-essential cmake g++-11
```
</details>

To use TAOBench, you will need to additionally install database-specific
libraries. Details can be found in each driver's documentation.

### Build
Clone TAOBench's [Git repository](https://github.com/audreyccheng/taobench) and
build with the following:
```shell
git clone https://github.com/audreyccheng/taobench.git
cmake . -DWITH_<dbname>=ON
make
```
Supply any of the following CMake flags: `WITH_CRDB`, `WITH_MYSQL`,
`WITH_SPANNER`, `WITH_YUGABYTE` to build the respective drivers.

You should now have the `taobench` executable.

## Usage
See [USAGE.md](USAGE.md).

Currently, we provide adapter layers for the following services:

- [Cloud Spanner](https://cloud.google.com/spanner)
- MySQL and MySQL-compatible databases
  - [PlanetScale](https://planetscale.com/)
  - [TiDB](https://tidbcloud.com/)
- Postgres-compatible databases
  - [CockroachDB](https://www.cockroachlabs.com/get-started-cockroachdb/)
  - [YugabyteDB](https://www.yugabyte.com/)

## Credit
This is a fork of [YCSB-cpp](https://github.com/ls4154/YCSB-cpp).
