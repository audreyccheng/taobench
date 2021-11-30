cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
project(benchmark)
find_package(google_cloud_cpp_spanner REQUIRED)
include_directories(src)
include_directories(/usr/local/include)
include_directories(spanner_db)
file(GLOB SOURCES
  src/*.h
  src/*.cc
  spanner_db/*.h
  spanner_db/*.cc
)
add_executable(benchmark ${SOURCES})
target_link_libraries(benchmark google-cloud-cpp::spanner)
