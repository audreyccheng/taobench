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

  Status Read(const std::string &table, const std::vector<Field> & key,
              const std::vector<std::string> *fields, std::vector<Field> &result);

  Status Scan(const std::string &table, const std::vector<Field> &key,
              int record_count, const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result);

  Status Update(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

  Status Insert(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);
  
  Status BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                     const std::vector<std::vector<Field>> &values);

  Status BatchRead(const std::string &table, int start_index, int end_index,
                   std::vector<std::vector<Field>> &result);

  Status Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

  Status Execute(const DB_Operation &operation, std::vector<std::vector<Field>> &result, bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations, std::vector<std::vector<Field>> &results, bool read_only);

 private:
  std::mutex mutex_;
  pqxx::connection *conn_;
  std::string object_table_;
  std::string edge_table_;

  pqxx::result DoRead(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                              const std::vector<std::string> *fields);

  pqxx::result DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
                              const std::vector<std::string> *fields);

  pqxx::result DoUpdate(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  pqxx::result DoInsert(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  pqxx::result DoDelete(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  Status BatchInsertObjects(const std::string &table, const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values);

  Status BatchInsertEdges(const std::string &table, const std::vector<std::vector<Field>> &keys,
                          const std::vector<std::vector<Field>> &values);

};

DB *NewCrdbDB();

} // benchmark

#endif // CRDB_DB_H_
