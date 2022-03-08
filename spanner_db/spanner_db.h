// #pragma once

#include "db.h"
#include "properties.h"
#include "db_factory.h"
#include "timer.h"
#include <iostream>
#include <string>
#include <mutex>
#include "google/cloud/spanner/client.h"

namespace benchmark {

namespace spanner = ::google::cloud::spanner;
using ::google::cloud::StatusOr;

class SpannerDB : public DB {
public:

  SpannerDB()
  {
  }
  
  void Init();

  void Cleanup();

  Status Read(DataTable table,
              const std::vector<DB::Field> &key,
              std::vector<TimestampValue> &buffer);

  Status Scan(DataTable table,
              const std::vector<DB::Field> &key,
              int n,
              std::vector<TimestampValue> &buffer);

  Status Update(DataTable table,
                const std::vector<DB::Field> &key,
                TimestampValue const & value);

  Status Insert(DataTable table,
                const std::vector<DB::Field> &key,
                TimestampValue const & value);

  Status Delete(DataTable table,
                const std::vector<DB::Field> &key,
                TimestampValue const & value);

  Status Execute(const DB_Operation &operation,
                 std::vector<TimestampValue> & read_buffer,
                 bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> & operations,
                            std::vector<TimestampValue> & read_buffer,
                            bool read_only);

  Status BatchInsert(DataTable table,
                     const std::vector<std::vector<Field>> & keys,
                     const std::vector<TimestampValue> & values);

  Status BatchRead(DataTable table, 
                   std::vector<DB::Field> const & floor_key,
                   std::vector<DB::Field> const & ceil_key,
                   int n, 
                   std::vector<std::vector<DB::Field>> &key_buffer);                     

private:

  struct ConnectorInfo {

    ConnectorInfo(utils::Properties const & props);

    spanner::Client client;
  };

  ConnectorInfo *info;

  Status BatchInsertObjects(DataTable table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<TimestampValue> &timevals);

  Status BatchInsertEdges(DataTable table,
                          const std::vector<std::vector<Field>> &keys,
                          const std::vector<TimestampValue> &timevals);

};

DB *NewMySqlDB();

} // benchmark
