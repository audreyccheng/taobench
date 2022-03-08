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
#include <cassandra.h>
#include <vector>

// namespace benchmark {

// class YcqlDB : public DB {
// public:

//   void Init();

//   void Cleanup();

//   Status Read(const std::string &table, const std::vector<Field> &key,
//               const std::vector<std::string> *fields, std::vector<Field> &result);

//   Status Scan(const std::string &table, const std::vector<Field> &key, int len,
//               const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result, int start, int end);

//   Status Update(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

//   Status Insert(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

//   Status Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values);

//   Status BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
//                              const std::vector<std::vector<Field>> &values);

//   Status BatchRead(const std::string &table, int start_index, int end_index,
//                    std::vector<std::vector<Field>> &result);       

//   Status Execute(const DB_Operation &operation,
//                  std::vector<std::vector<Field>> &result, // for reads
//                  bool txn_op = false);

//   Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
//                             std::vector<std::vector<Field>> &results, bool read_only);

// private:
//   CassCluster* cluster;
//   CassSession* session;

//   // Prepared statements
//   CassFuture* read_object_future;
//   CassFuture* read_edge_future;
//   CassFuture* scan_object_future;
//   CassFuture* scan_edge_future;
//   CassFuture* update_object_future;
//   CassFuture* update_edge_future;
//   CassFuture* insert_object_future;
//   CassFuture* insert_bi_future;
//   CassFuture* insert_other_future;
//   CassFuture* insert_unique_future;
//   CassFuture* insert_biunique_future;
//   CassFuture* delete_object_future;
//   CassFuture* delete_edge_future;

//   std::mutex mu_;
//   std::string object_table_;
//   std::string edge_table_;

//   Status BatchInsertObjects(const std::string & table,
//                             const std::vector<std::vector<Field>> &keys,
//                             const std::vector<std::vector<Field>> &values);

//   Status BatchInsertEdges(const std::string & table,
//                           const std::vector<std::vector<Field>> &keys,
//                           const std::vector<std::vector<Field>> &values);

//   // helper functions to generate queries for transactions
//   std::string readQuery(const std::string &table, const std::vector<Field> &key,
//                         const std::vector<std::string> *fields);

//   std::string scanQuery(const std::string &table, const std::vector<Field> &key, int len,
//                         const std::vector<std::string> *fields);

//   std::string updateQuery(const std::string &table, const std::vector<Field> &key,
//                           const std::vector<Field> &values);

//   std::string insertQuery(const std::string &table, const std::vector<Field> &key,
//                           const std::vector<Field> &values);

//   std::string deleteQuery(const std::string &table, const std::vector<Field> &key,
//                           const std::vector<Field> &values);

// };

// DB *NewYsqlDB();

// } // benchmark

#endif // YBDB_DB_H_
