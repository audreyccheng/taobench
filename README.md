# TAOBench
A distributed database benchmark based on Meta's social graph workload.

## Overview

TAOBench is a benchmark based on Meta's social networking workload for distributed databases.
Specifically, TAOBench captures the requests patterns of [TAO](https://www.usenix.org/system/files/conference/atc13/atc13-bronson.pdf), the social graph store at Meta. TAOBench can both accurately model production workloads and generate emergent application behavior at flexible scale.

### Use Cases
TAOBench can be configured and used for many purposes depending on developer needs. The following are few examples of how the benchmark has been used.

1. **Analyzing new workloads**: TAOBench can be used to measure the performance of novel workloads, including ones that are difficult or infeasible to run in production. For example, this benchmark has been used at Meta to test new transactional workloads and was able to succesfully predict error rates that were later observed in production. It has also been used to quantify the impact of high fan-out transactions.

2. **Assessing system reliability**: TAOBench can reproduce workloads on demand to evaluate how a system performs under stressful scenarios (e.g., network delays, regional overload, and disaster recovery). At Meta, engineers used this benchmark to understand TAO performance and error rates when locks were held for extended periods of time.

3. **Evaluating new features**: TAOBench enables developers to test new features, optimizations, and more at flexible scale. System engineers at Meta leveraged this benchmark to measure how additional read-modify-write methods could increase the success rates of requests.

### Workload Features
TAOBench's workloads, which are derived from Meta's production requests, have a number of interesting features. For details, check out our [VLDB paper](https://www.vldb.org/pvldb/vol15/p1965-cheng.pdf).

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
