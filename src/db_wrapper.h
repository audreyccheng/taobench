#ifndef DB_WRAPPER_H_
#define DB_WRAPPER_H_

#include <string>
#include <vector>

#include "db.h"
#include "measurements.h"
#include "timer.h"
#include "utils.h"

namespace benchmark {

class DBWrapper : public DB {
 public:
  DBWrapper(DB *db, Measurements *measurements) :
    db_(db) , measurements_(measurements) {}
  ~DBWrapper() {
    delete db_;
  }
  void Init() {
    db_->Init();
  }
  void Cleanup() {
    db_->Cleanup();
  }
  Status Read(const std::string &table, const std::vector<Field> &key,
              const std::vector<std::string> *fields, std::vector<Field> &result) {
    throw std::invalid_argument("DBWrapper Read method should never be called.");
  }

  Status Scan(const std::string &table, const std::vector<Field> &key, int record_count,
              const std::vector<std::string> *fields, std::vector<std::vector<Field>> &result) {
    return db_->Scan(table, key, record_count, fields, result);
  }

  Status Update(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
    throw std::invalid_argument("DBWrapper Update method should never be called.");
  }

  Status Insert(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
    throw std::invalid_argument("DBWrapper Insert method should never be called.");
  }

  Status BatchInsert(const std::string &table, 
                     const std::vector<std::vector<Field>> &keys, 
                     const std::vector<std::vector<Field>> &values) 
  {
    return db_->BatchInsert(table, keys, values);
  }

  Status BatchRead(const std::string &table, int start_index, int end_index,
                   std::vector<std::vector<Field>> &values)
  {
    return db_->BatchRead(table, start_index, end_index, values);
  }

  Status Delete(const std::string &table, const std::vector<Field> &key,
                const std::vector<Field> &values) {
    throw std::invalid_argument("DBWrapper Delete method should never be called.");
  }

  Status Execute(const DB_Operation &operation,
                 std::vector<std::vector<Field>> &result,
                 bool txn_op = false) {
    timer_.Start();
    Status s = db_->Execute(operation, result);
    uint64_t elapsed = timer_.End();
    if (s == Status::kOK) {
      measurements_->Report(operation.operation, elapsed);
    }
    return s;
  }

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<std::vector<Field>> &results,
                            bool read_only = false)
  {
    timer_.Start();
    Status s = db_->ExecuteTransaction(operations, results, read_only);
    uint64_t elapsed = timer_.End();
    assert(!operations.empty());
    if (s != Status::kOK) {
      return s;
    }
    if (operations[0].operation == Operation::READ || operations[0].operation == Operation::SCAN) {
      measurements_->Report(Operation::READTRANSACTION, elapsed);
    } else {
      measurements_->Report(Operation::WRITETRANSACTION, elapsed);
    }
    return s;
  }
 private:
  DB *db_;
  Measurements *measurements_;
  utils::Timer<uint64_t, std::nano> timer_;
};

} // benchmark

#endif // DB_WRAPPER_H_
