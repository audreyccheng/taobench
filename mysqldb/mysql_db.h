#pragma once

#include "db.h"
#include "properties.h"
#include "db_factory.h"
#include "timer.h"
#include <iostream>
#include <string>
#include <mutex>
#include <superior_mysqlpp.hpp>


namespace benchmark {

using PreparedStatement = SuperiorMySqlpp::DynamicPreparedStatement<true, SuperiorMySqlpp::ValidateMetadataMode::ArithmeticPromotions, SuperiorMySqlpp::ValidateMetadataMode::Same, false>;

class MySqlDB : public DB {
public:

  MySqlDB()
  {
  }
  
  void Init();

  void Cleanup();

  Status Read(DataTable table, const std::vector<Field> & key,
              std::vector<TimestampValue> &buffer);

  Status Scan(DataTable table, const std::vector<Field> & key, int n,
              std::vector<TimestampValue> &buffer);

  Status Update(DataTable table, const std::vector<Field> &key,
                TimestampValue const & value);

  Status Insert(DataTable table, const std::vector<Field> &key,
                TimestampValue const & value);

  Status BatchInsert(DataTable table, const std::vector<std::vector<Field>> &keys,
                     std::vector<TimestampValue> const & values);

  Status BatchRead(DataTable table, const std::vector<Field> &floor_key,
                   const std::vector<Field> &ceiling_key, int n,
                   std::vector<std::vector<Field>> &key_buffer);

  Status Delete(DataTable table, const std::vector<Field> &key,
                TimestampValue const & value);

  Status Execute(const DB_Operation &operation,
                 std::vector<TimestampValue> &read_buffer,
                 bool txn_op = false);

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only);

private:
  Status BatchInsertObjects(const std::vector<std::vector<Field>> &keys,
                            const std::vector<TimestampValue> &timeval);

  Status BatchInsertEdges(const std::vector<std::vector<Field>> &keys,
                          const std::vector<TimestampValue> &timeval);

  struct PreparedStatements {

    PreparedStatements(utils::Properties const & props);

    SuperiorMySqlpp::Connection sql_connection_;
    PreparedStatement read_object;
    PreparedStatement read_edge;
    PreparedStatement insert_object;
    PreparedStatement insert_other, insert_unique, insert_bidirectional, insert_unique_and_bidirectional;
    PreparedStatement delete_object, delete_edge;
    PreparedStatement update_object, update_edge;
  };

  PreparedStatements *statements;
  std::mutex mutex_;
  static int ref_cnt_;

};

DB *NewMySqlDB();

} // benchmark
