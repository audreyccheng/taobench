cmake_minimum_required(VERSION 3.7)

project(benchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "g++-11")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

option(WITH_CRDB OFF)
option(WITH_MYSQL OFF)
option(WITH_SPANNER OFF)
option(WITH_YBSQL OFF)

include_directories(src)
file(GLOB SOURCES src/*.h src/*.cc)
add_executable(benchmark ${SOURCES})

if(WITH_CRDB)
  include_directories(crdb)
  target_sources(benchmark PRIVATE
    crdb/crdb_db.h
    crdb/crdb_db.cc)
  target_link_libraries(benchmark -lpqxx)
  target_link_libraries(benchmark -lpq)
endif()

if(WITH_MYSQL)
  include_directories(mysqldb)
  target_sources(benchmark PRIVATE
    mysqldb/mysql_db.h
    mysqldb/mysql_db.cc)
  target_link_libraries(benchmark -lmysqlclient)
endif()

if(WITH_SPANNER)
  include_directories(spanner_db)
  target_sources(benchmark PRIVATE
    spanner_db/spanner_db.h
    spanner_db/spanner_db.cc)
  target_link_libraries(benchmark google-cloud-cpp::spanner)
endif()

if(WITH_YBSQL)
  include_directories(ybsql_db)
  target_sources(benchmark PRIVATE
    ybsql_db/ybsql_db.h
    ybsql_db/ybsql_db.cc)
  target_link_libraries(benchmark -lpqxx)
  target_link_libraries(benchmark -lpq)
endif()
