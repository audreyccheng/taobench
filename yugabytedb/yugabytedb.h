#pragma once

#include "db.h"
#include "properties.h"
#include "db_factory.h"
#include "timer.h"
#include <iostream>
#include <string>
#include <mutex>
#include <pqxx/pqxx>
#include <vector>

namespace benchmark {

class YugabyteDB : public DB {
public:
  void Init();

  void Cleanup();

  Status Read(DataTable table, const std::vector<DB::Field> &key,
              std::vector<TimestampValue> &buffer);

  Status Scan(DataTable table, const std::vector<DB::Field> &key, int n,
              std::vector<TimestampValue> &buffer);

  Status Update(DataTable table, const std::vector<DB::Field> &key,
                TimestampValue const &value);

  Status Insert(DataTable table, const std::vector<DB::Field> &key,
                TimestampValue const &value);

  Status Delete(DataTable table, const std::vector<DB::Field> &key,
                TimestampValue const &value);

  Status Execute(const DB_Operation &operation,
                 std::vector<TimestampValue> &read_buffer, bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only);

  Status BatchInsert(DataTable table,
                     const std::vector<std::vector<Field>> &keys,
                     const std::vector<TimestampValue> &values);

  Status BatchRead(DataTable table, std::vector<DB::Field> const &floor_key,
                   std::vector<DB::Field> const &ceil_key, int n,
                   std::vector<std::vector<DB::Field>> &key_buffer);

private:
  pqxx::connection *ysql_conn_;
  std::mutex mu_;
  std::string object_table_;
  std::string edge_table_;

  /* Helper functions to execute the prepared statements done in Init */
  pqxx::result DoRead(pqxx::transaction_base &tx, DataTable table,
                      const std::vector<Field> &key);

  pqxx::result DoUpdate(pqxx::transaction_base &tx, DataTable table,
                        const std::vector<Field> &key,
                        TimestampValue const &value);

  pqxx::result DoInsert(pqxx::transaction_base &tx, DataTable table,
                        const std::vector<Field> &key,
                        const TimestampValue &timeval);

  pqxx::result DoDelete(pqxx::transaction_base &tx, DataTable table,
                        const std::vector<Field> &key,
                        const TimestampValue &timeval);

  Status BatchInsertObjects(DataTable table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<TimestampValue> &timevals);

  Status BatchInsertEdges(DataTable table,
                          const std::vector<std::vector<Field>> &keys,
                          const std::vector<TimestampValue> &timevals);

  Status ExecuteTransactionPrepared(const std::vector<DB_Operation> &operations,
                                    std::vector<TimestampValue> &results,
                                    bool read_only);

  Status ExecuteTransactionBatch(const std::vector<DB_Operation> &operations,
                                 std::vector<TimestampValue> &results,
                                 bool read_only);

  std::string ReadBatchQuery(const std::vector<DB_Operation> &read_ops);

  std::string ScanBatchQuery(const std::vector<DB_Operation> &scan_ops);

  std::string InsertBatchQuery(const std::vector<DB_Operation> &insert_ops);

  std::string UpdateBatchQuery(const std::vector<DB_Operation> &update_ops);

  std::string DeleteBatchQuery(const std::vector<DB_Operation> &delete_ops);
};

DB *NewYugabyteDB();

} // namespace benchmark
