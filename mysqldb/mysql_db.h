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

  Status Read(const std::string &table,
              const std::vector<Field> &key,
              const std::vector<std::string> *fields,
              std::vector<Field> &result);

  Status Scan(const std::string &table,
              const std::vector<Field> &key,
              int record_count,
              const std::vector<std::string> *fields,
              std::vector<std::vector<Field>> &result);

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
                            bool read_only = false);

private:

  Status BatchInsertObjects(const std::string & table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values);

  Status BatchInsertEdges(const std::string & table,
                          const std::vector<std::vector<Field>> &keys,
                          const std::vector<std::vector<Field>> &values);

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
