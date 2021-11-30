#include "test_db.h"
#include "db_factory.h"

using std::cout;
using std::endl;

namespace benchmark {

std::mutex TestDB:: mutex_;

void TestDB::Init() {
  std::lock_guard<std::mutex> lock(mutex_);
}

Status TestDB::Read(const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields, std::vector<Field> &result) {
  if (key.size() == 0) {
    throw std::invalid_argument("Attempting to read key array with no key");
  }
  cout << "READ " << table << " (" << key[0].value;
  for (size_t i = 1; i < key.size(); ++i) {
    cout << ',' << key[i].value;
  }
  cout << ')';
  if (fields) {
    cout << " [ ";
    for (auto f : *fields) {
      cout << f << ' ';
    }
    cout << ']' << endl;
  } else {
    cout  << " < all fields >" << endl;
  }
  return Status::kOK;
}

Status TestDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields,
                         std::vector<std::vector<Field>> &result,
                         const std::vector<Field> &limit) {
  if (key.size() == 0) {
    throw std::invalid_argument("Attempting to scan key array with no key");
  }
  cout << "SCAN " << table << " (" << key[0].value;
  for (size_t i = 1; i < key.size(); ++i) {
    cout << ',' << key[i].value;
  }
  cout << ") " << len;
  if (fields) {
    cout << " [ ";
    for (auto f : *fields) {
      cout << f << ' ';
    }
    cout << ']' << endl;
  } else {
    cout  << " < all fields >" << endl;
  }
  return Status::kOK;
}

Status TestDB::Update(const std::string &table, const std::vector<Field> &key,
                           const std::vector<Field> &values) {
  if (key.size() == 0) {
    throw std::invalid_argument("Attempting to update key array with no key");
  }
  cout << "UPDATE " << table << " (" << key[0].value;
  for (size_t i = 1; i < key.size(); ++i) {
    cout << ',' << key[i].value;
  }
  cout << ") [ ";
  for (auto v : values) {
    cout << v.name << '=' << v.value << ' ';
  }
  cout << ']' << endl;
  return Status::kOK;
}

Status TestDB::Insert(const std::string &table, const std::vector<Field> &key,
                          const std::vector<Field> &values) {
  if (key.size() == 0) {
    throw std::invalid_argument("Attempting to insert key array with no key");
  }
  cout << "INSERT " << table << " (" << key[0].value;
  for (size_t i = 1; i < key.size(); ++i) {
    cout << ',' << key[i].value;
  }
  cout << ") [ ";
  for (auto v : values) {
    cout << v.name << '=' << v.value << ' ';
  }
  cout << ']' << endl;
  return Status::kOK;
}

Status TestDB::BatchInsert(const std::string & table, const std::vector<std::vector<Field>> &keys, 
                           const std::vector<std::vector<Field>> &values)
{
  return Status::kNotImplemented;
}

Status TestDB::BatchRead(const std::string &table, int start_index, int end_index,
                         std::vector<std::vector<Field>> &values)
{
  return Status::kNotImplemented;
}

Status TestDB::Delete(const std::string &table, const std::vector<Field> &key,
                      const std::vector<Field> &values) {
  if (key.size() == 0) {
    throw std::invalid_argument("Attempting to insert key array with no key");
  }
  cout << "DELETE " << table << " (" << key[0].value;
  for (size_t i = 1; i < key.size(); ++i) {
    cout << ',' << key[i].value;
  }
  cout << ')' << endl;
  return Status::kOK;
}

Status TestDB::Execute(const DB_Operation & op,
                           std::vector<std::vector<Field>> &results, bool txn_op) {
  // On reads, execute will add one element to the results vector, corresponding to the 
  // row it read. This enables compatibility with transactions; for non-transaction operations,
  // we just get a results vector with one element.
  if (!txn_op) {
    std::lock_guard<std::mutex> lock(mutex_);
  }
  switch (op.operation) {
    case Operation::READ: {
      results.emplace_back();
      std::vector<std::string> fields;
      for (auto const & field : op.fields) {
        fields.push_back(field.name);
      }
      Read(op.table, op.key, &fields, results.back());
      break;
    }
    case Operation::DELETE:
      Delete(op.table, op.key, op.fields);
      break;
    case Operation::UPDATE:
      Update(op.table, op.key, op.fields);
      break;
    case Operation::INSERT:
      Insert(op.table, op.key, op.fields);
      break;
    default:
      return Status::kOK;
  }
  return Status::kOK;
}

Status TestDB::ExecuteTransaction(const std::vector<DB_Operation> & ops,
                                  std::vector<std::vector<Field>> &results,
                                  bool read_only) {
  std::lock_guard<std::mutex> lock(mutex_);
  cout << "BEGIN TRANSACTION" << endl;
  for (auto const & op : ops) {
    Execute(op, results, true);
  }
  cout << "END TRANSACTION" << endl;
  return Status::kOK;
}

DB *NewTestDB() {
  return new TestDB;
}

const bool registered = DBFactory::RegisterDB("test", NewTestDB);

} // benchmark
