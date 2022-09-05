# Cloud Spanner

## Build Instructions

Follow the CMake instructions on the Spanner quickstart guide (https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/quickstart) to install the Spanner client libraries using vcpkg.

Then run: 

```shell
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .
make
``` 

## Schema Setup

On your Spanner instance, create a database with the following schema:

```sql
CREATE TABLE objects(
    id INT64,
    timestamp INT64,
    value STRING(150),
) PRIMARY KEY (id);

CREATE TABLE edges(
    id1 INT64,
    id2 INT64,
    type INT64,
    timestamp INT64,
    value STRING(150),
) PRIMARY KEY (id1, id2, type);
```

## Instance Configuration

The Spanner `project-id`, `instance-id`, and `database` can be configured in the `spanner_db/spanner.properties` file.
