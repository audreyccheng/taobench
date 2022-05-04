cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++-11")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
project(benchmark)
include_directories(/usr/local/include)
include_directories(src)
include_directories(crdb)
#include_directories(spanner_db)
include_directories(ybsql_db)
link_directories(/usr/local/lib)
file(GLOB SOURCES
  src/*.h
  src/*.cc
  crdb/*.h
  crdb/*.cc
  #spanner_db/*.h
  #spanner_db/*.cc
  ybsql_db/*.h
  ybsql_db/*.cc
)
add_executable(benchmark ${SOURCES})
target_link_libraries(benchmark -lpqxx)
target_link_libraries(benchmark -lpq)
#target_link_libraries(benchmark google-cloud-cpp::spanner)

