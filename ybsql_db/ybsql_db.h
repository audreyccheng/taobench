#ifndef YBDB_DB_H_
#define YBDB_DB_H_

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

class YsqlDB : public DB {
public:

  void Init();

  void Cleanup();

  Status Read(const std::string &table, const std::vector<Field> &key,
              const std::vector<std::string> *fields, std::vector<Field> &result);

  Status Scan(const std::string &table, const std::vector<Field> &key, int len,
              const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result, int start, int end);

  Status Update(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

  Status Insert(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

  Status Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

  Status BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                             const std::vector<std::vector<Field>> &values);

  Status BatchRead(const std::string &table, int start_index, int end_index,
                   std::vector<std::vector<Field>> &result);       

  Status Execute(const DB_Operation &operation,
                 std::vector<std::vector<Field>> &result, // for reads
                 bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<std::vector<Field>> &results, bool read_only);

private:
  pqxx::connection* ysql_conn_;
  std::mutex mu_;
  std::string object_table_;
  std::string edge_table_;

  /* Helper functions to execute the prepared statements done in Init */
  pqxx::result DoRead(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                              const std::vector<std::string> *fields);

  pqxx::result DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
                              const std::vector<std::string> *fields, int start, int end);

  pqxx::result DoUpdate(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  pqxx::result DoInsert(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  pqxx::result DoDelete(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                                const std::vector<Field> &values);

  Status BatchInsertObjects(const std::string & table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values);

  Status BatchInsertEdges(const std::string & table,
                          const std::vector<std::vector<Field>> &keys,
                          const std::vector<std::vector<Field>> &values);
};

DB *NewYsqlDB();

} // benchmark

#endif // YBDB_DB_H_
