cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++-11")
project(benchmark)
include_directories(/usr/local/include)
include_directories(src)
include_directories(crdb)
include_directories(mysqldb)
link_directories(/usr/local/lib)
file(GLOB SOURCES
  src/*.h
  src/*.cc
  crdb/*.h
  crdb/*.cc
)
add_executable(benchmark ${SOURCES})
target_link_libraries(benchmark -lpqxx)
target_link_libraries(benchmark -lpq)
SET(CMAKE_CXX_FLAGS -pthread)
