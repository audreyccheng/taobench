#ifndef TEST_DB_H_
#define TEST_DB_H_

#include "db.h"
#include "properties.h"

#include <iostream>
#include <string>
#include <mutex>

namespace benchmark {

class TestDB : public DB {
 public:
  void Init() override;

  Status Read(const std::string &table, const std::vector<Field> &key,
              const std::vector<std::string> *fields, std::vector<Field> &result) override;

  Status Scan(const std::string &table, const std::vector<Field> &key, int len,
              const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result, int start, int end) override;

  Status Update(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) override;

  Status Insert(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) override;

  Status BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                     const std::vector<std::vector<Field>> & values) override;

  Status BatchRead(const std::string &table, int start_index, int end_index,
                           std::vector<std::vector<Field>> &values) override;

  Status Delete(const std::string &table, const std::vector<Field> &key,
                const std::vector<Field> &values) override;

  Status Execute(const DB_Operation & op,
                 std::vector<std::vector<Field>> &result,
                 bool txn_op = false) override;

  Status ExecuteTransaction(const std::vector<DB_Operation> &ops,
                            std::vector<std::vector<Field>> &results,
                            bool read_only = false) override;

 private:
  static std::mutex mutex_;
};

DB *NewTestDB();

} // benchmark

#endif // TEST_DB_H_
