#pragma once

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

  Status Read(const std::string &table,
              const std::vector<Field> &key,
              const std::vector<std::string> *fields,
              std::vector<Field> &result);

  Status Scan(const std::string &table,
              const std::vector<Field> &key,
              int record_count,
              const std::vector<std::string> *fields,
              std::vector<std::vector<Field>> &result,
              const std::vector<Field> &limit);

  Status Update(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values);

  Status Insert(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values);

  Status BatchInsert(const std::string & table,
                     const std::vector<std::vector<Field>> &keys,
                     const std::vector<std::vector<Field>> &values);

  Status BatchRead(const std::string &table, int start_index, int end_index,
                   std::vector<std::vector<Field>> &result);                     

  Status Delete(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values);

  Status Execute(const DB_Operation &operation,
                 std::vector<std::vector<Field>> & result,
                 bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> & operations,
                            std::vector<std::vector<Field>> & result,
                            bool read_only);

private:

  struct ConnectorInfo {

    ConnectorInfo(utils::Properties const & props);

    spanner::Client client;
  };

  ConnectorInfo *info;

  Status Read(const std::string &table,
              const std::vector<Field> &key,
              const std::vector<std::string> *fields,
              std::vector<Field> &result,
              spanner::Transaction txn);

  Status Update(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values,
                spanner::Transaction txn);

  Status Insert(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values,
                spanner::Transaction txn);

  Status Delete(const std::string &table,
                const std::vector<Field> &key,
                const std::vector<Field> &values,
                spanner::Transaction txn);

  Status ExecuteWithTransaction(const DB_Operation &operation,
                 std::vector<std::vector<Field>> & result,
                 spanner::Transaction txn);

  Status BatchInsertObjects(const std::string & table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values);

  Status BatchInsertEdges(const std::string & table,
                          const std::vector<std::vector<Field>> &keys,
                          const std::vector<std::vector<Field>> &values);

};

DB *NewMySqlDB();

} // benchmark
