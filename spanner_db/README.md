# Running TAOBench on Spanner

## Build Instructions

Follow the CMake instructions on the Spanner quickstart guide (https://github.com/googleapis/google-cloud-cpp/tree/main/google/cloud/spanner/quickstart) to install the Spanner client libraries using vcpkg. Then set up the following CMakeLists.txt file at the root directory level:

```
cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
project(benchmark)
find_package(google_cloud_cpp_spanner REQUIRED)
include_directories(src)
include_directories(/usr/local/include) # Or your `include` directory
include_directories(spanner_db)
file(GLOB SOURCES
  src/*.h
  src/*.cc
  spanner_db/*.h
  spanner_db/*.cc
)
add_executable(benchmark ${SOURCES})
target_link_libraries(benchmark google-cloud-cpp::spanner)
```

Then run: 

```
cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release .
make
``` 

## Schema Setup

On your Spanner instance, create a database with the following schema:

```
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

## Running TAOBench on Spanner

Pass the flags `-db spanner` and `-P spanner_db/spanner.properties` to the benchmark executable. Please refer to TAOBench documentation to find general instructions for running the benchmark.
