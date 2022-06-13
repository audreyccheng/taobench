#ifndef DB_WRAPPER_H_
#define DB_WRAPPER_H_

#include <string>
#include <vector>

#include "db.h"
#include "measurements.h"
#include "timer.h"
#include "utils.h"

namespace benchmark {

// Wrapper Class around DB; times and logs each Execute and ExecuteTransaction operation.
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
  Status Read(DataTable table, const std::vector<Field> &key,
              std::vector<TimestampValue> &buffer) {
    throw std::invalid_argument("DBWrapper Read method should never be called.");
  }

  Status Scan(DataTable table, const std::vector<Field> &key, int n,
              std::vector<TimestampValue> &buffer) {
    throw std::invalid_argument("DBWrapper Scan method should never be called.");
  }

  Status Update(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Update method should never be called.");
  }

  Status Insert(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Insert method should never be called.");
  }

  Status Delete(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
    throw std::invalid_argument("DBWrapper Delete method should never be called.");
  }

  Status Execute(const DB_Operation &operation,
                 std::vector<TimestampValue> &read_buffer,
                 bool txn_op = false) {
    timer_.Start();
    Status s = db_->Execute(operation, read_buffer, txn_op);
    uint64_t elapsed = timer_.End();
    if (s == Status::kOK) {
      measurements_->Report(operation.operation, elapsed);
    }
    return s;
  }

  Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                            std::vector<TimestampValue> &read_buffer,
                            bool read_only = false)
  {
    timer_.Start();
    Status s = db_->ExecuteTransaction(operations, read_buffer, read_only);
    uint64_t elapsed = timer_.End();
    assert(!operations.empty());
    if (s != Status::kOK) {
      return s;
    }
    if (read_only) {
      measurements_->Report(Operation::READTRANSACTION, elapsed);
    } else {
      measurements_->Report(Operation::WRITETRANSACTION, elapsed);
    }
    return s;
  }

  Status BatchInsert(DataTable table, 
                     const std::vector<std::vector<Field>> &keys, 
                     const std::vector<TimestampValue> &values) 
  {
    return db_->BatchInsert(table, keys, values);
  }

  Status BatchRead(DataTable table,
                   const std::vector<Field> & floor,
                   const std::vector<Field> & ceil,
                   int n,
                   std::vector<std::vector<Field>> &key_buffer)
  {
    return db_->BatchRead(table, floor, ceil, n, key_buffer);
  }

 private:
  DB *db_;
  Measurements *measurements_;
  utils::Timer<uint64_t, std::nano> timer_;
};

} // benchmark

#endif // DB_WRAPPER_H_
