cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++-11")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
project(benchmark)
include_directories(/usr/local/include)
include_directories(/path/to/SuperiorMySqlpp/include)
include_directories(src)
include_directories(mysqldb)
link_directories(/usr/local/lib)
link_directories(/path/to/mysql@5.7/lib)
file(GLOB SOURCES
  src/*.h
  src/*.cc
  mysqldb/*.h
  mysqldb/*.cc
)
add_executable(benchmark ${SOURCES})
target_link_libraries(benchmark -lmysqlclient)
