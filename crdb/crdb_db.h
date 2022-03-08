#ifndef CRDB_DB_H_
#define CRDB_DB_H_

#include "db.h"
#include "properties.h"

#include <iostream>
#include <string>
#include <mutex>
#include <stdexcept>

#include <pqxx/pqxx>

namespace benchmark {

class CrdbDB : public DB {
 public:
  void Init();

  void Cleanup();

  Status Read(DataTable table, const std::vector<Field> & key, std::vector<TimestampValue> &buffer);

  Status Scan(DataTable table, const std::vector<Field> & key, int n, std::vector<TimestampValue> &buffer);

  Status Update(DataTable table, const std::vector<Field> &key, TimestampValue const & value);

  Status Insert(DataTable table, const std::vector<Field> &key, TimestampValue const & value);
  

  Status Delete(DataTable table, const std::vector<Field> &key,
                        TimestampValue const & value);

  Status Execute(const DB_Operation &operation,
                         std::vector<TimestampValue> &read_buffer, // for reads
                         bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer, bool read_only);

  Status BatchInsert(DataTable table, const std::vector<std::vector<Field>> &keys,
                     std::vector<TimestampValue> const & values);

  Status BatchRead(DataTable table, const std::vector<Field> &floor_key,
                   const std::vector<Field> &ceiling_key, int n, std::vector<std::vector<Field>> &key_buffer);

 private:
  std::mutex mutex_;
  pqxx::connection *conn_;
  std::string object_table_;
  std::string edge_table_;

  pqxx::result DoRead(pqxx::transaction_base &tx, const DataTable table, const std::vector<Field> &key);

  // pqxx::result DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
  //                             const std::vector<std::string> *fields, const std::vector<Field> &limit);

  pqxx::result DoUpdate(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, TimestampValue const &value);

  pqxx::result DoInsert(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue & value);

  pqxx::result DoDelete(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue &value);

  Status BatchInsertObjects(DataTable table, const std::vector<std::vector<Field>> &keys,
                                  const std::vector<TimestampValue> &values);

  Status BatchInsertEdges(DataTable table, const std::vector<std::vector<Field>> &keys,
                                  const std::vector<TimestampValue> &values);

  Status ExecuteTransactionBatch(const std::vector<DB_Operation> &operations, std::vector<TimestampValue> &results, bool read_only);

  Status ExecuteTransactionPrepared(const std::vector<DB_Operation> &operations, std::vector<TimestampValue> &results, bool read_only);
 
  std::string GenerateMergedReadQuery(const std::vector<DB_Operation> &read_operations);

  std::string GenerateMergedInsertQuery(const std::vector<DB_Operation> &insert_operations);

  std::string GenerateMergedUpdateQuery(const std::vector<DB_Operation> &update_operations);

  std::string GenerateMergedDeleteQuery(const std::vector<DB_Operation> &delete_operations);

};

DB *NewCrdbDB();

} // benchmark

#endif // CRDB_DB_H_
