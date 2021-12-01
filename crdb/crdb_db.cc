#include "crdb_db.h"
#include "db_factory.h"
#include <pqxx/pqxx>
#include <chrono>


using std::cout;
using std::endl;

namespace {
  const std::string CONNECTION_STRING = "crdb.connectionstring";
}

namespace benchmark {

void CrdbDB::Init() {
  std::lock_guard<std::mutex> lock(mutex_);
  const utils::Properties &props = *props_;
  std::string connectionstring = props.GetProperty(CONNECTION_STRING);
  if (CONNECTION_STRING == "") {
    throw utils::Exception("Incomplete login credentials in CRDB properties file");
  }

  conn_ = new pqxx::connection(connectionstring);


  // create prepared statements
  edge_table_ = props_->GetProperty("edge_table_", "edges");
  object_table_ = props_->GetProperty("object_table_", "objects");
  conn_->prepare("read_object", "SELECT timestamp, value FROM " + object_table_ + " WHERE id = $1");
  conn_->prepare("read_edge", "SELECT timestamp, value FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3");
  conn_->prepare("scan_object", "SELECT id FROM " + object_table_ + " WHERE id > $1 ORDER BY id LIMIT $2");
  conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE (id1, id2, type) > ($1, $2, $3) ORDER BY id1, id2, type LIMIT $4");
  conn_->prepare("update_object", "UPDATE " + object_table_ + " SET timestamp = $1, value = $2 WHERE id = $3 AND timestamp < $1");
  conn_->prepare("update_edge", "UPDATE " + edge_table_ + " SET timestamp = $1, value = $2 WHERE id1 = $3 AND id2 = $4 AND type = $5 AND timestamp < $1");
  conn_->prepare("insert_object", "INSERT INTO " + object_table_ + " (id, timestamp, value) SELECT $1, $2, $3");
  conn_->prepare("insert_edge_other", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'other', $3, $4"
                                      + " WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1 AND type = 'unique') OR (id1 = $1 AND type='unique_and_bidirectional') OR (id1 = $1 AND id2 = $2 AND type = 'bidirectional') OR (id1 = $2 AND id2 = $1))");
  conn_->prepare("insert_edge_bidirectional", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'bidirectional', $3, $4"
                                                + " WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1 AND type = 'unique') OR (id1 = $1 AND type='unique_and_bidirectional') OR (id1 = $1 AND id2 = $2 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'unique'))");
  conn_->prepare("insert_edge_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'unique', $3, $4"
                                          + " WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1) OR (id1 = $2 AND id2 = $1))");
  conn_->prepare("insert_edge_bi_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'unique_and_bidirectional', $3, $4"
                                            + " WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1) OR (id1 = $2 AND id2 = $1 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'unique'))");
  conn_->prepare("delete_object", "DELETE FROM " + object_table_ + " WHERE id = $1 AND timestamp < $2");
  conn_->prepare("delete_edge", "DELETE FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3 AND timestamp < $4");
  conn_->prepare("batch_read", "SELECT id1, id2, type FROM edges ORDER BY id1, id2, type LIMIT $1 OFFSET $2");
}

void CrdbDB::Cleanup() {
  conn_->close();
  delete conn_;
}

/*
  key always in the order {id1, id2, type} or {id1}
  fields always in the other {timestamp, value}
*/
Status CrdbDB::Read(const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields, std::vector<Field> &result) {

  std::lock_guard<std::mutex> lock(mutex_);

  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoRead(tx, table, key, fields);

    for (int i = 0; i < fields->size(); i++) {
      Field f((*fields)[i], (queryRes[0][i]).as<std::string>("NULL"));
      result.emplace_back(f);
    }


    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoRead(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields) {
  if (table == object_table_) {
    return tx.exec_prepared("read_object", key[0].value);
  } else if (table == edge_table_) {
    return tx.exec_prepared("read_edge", key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields,
                         std::vector<std::vector<Field>> &result) {
  assert(fields == nullptr);
  std::vector<std::string> hardcoded_fields{"id1", "id2", "type"};
  if (fields == nullptr) {
    fields = &hardcoded_fields;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoScan(tx, table, key, len, fields);

    for (auto row : queryRes) {
      std::vector<Field> oneRowVector;
      for (int j = 0; j < fields->size(); j++) {
        Field f((*fields)[j], (row)[j].as<std::string>("NULL"));
        oneRowVector.push_back(f);
      }
      result.push_back(oneRowVector);
    }

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields) {
  if (table == object_table_) {
    return tx.exec_prepared("scan_object", key[0].value, len);
  } else if (table == edge_table_) {
    return tx.exec_prepared("scan_edge", key[0].value, key[1].value, key[2].value, len);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Update(const std::string &table, const std::vector<Field> &key,
                           const std::vector<Field> &values) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoUpdate(tx, table, key, values);
    
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoUpdate(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == object_table_) {
    return tx.exec_prepared("update_object", values[0].value, values[1].value, key[0].value);
  } else if (table == edge_table_) {
    return tx.exec_prepared("update_edge", values[0].value, values[1].value, key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Insert(const std::string &table, const std::vector<Field> &key,
                           const std::vector<Field> &values) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoInsert(tx, table, key, values);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoInsert(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == object_table_) {
    return tx.exec_prepared("insert_object", key[0].value, values[0].value, values[1].value);
  } else if (table == edge_table_) {
    std::string type = key[2].value;
    if (type == "other") {
      return tx.exec_prepared("insert_edge_other", key[0].value, key[1].value, values[0].value, values[1].value);
    } else if (type == "bidirectional") {
      return tx.exec_prepared("insert_edge_bidirectional", key[0].value, key[1].value, values[0].value, values[1].value);
    } else if (type == "unique") {
      return tx.exec_prepared("insert_edge_unique", key[0].value, key[1].value, values[0].value, values[1].value);
    } else if (type == "unique_and_bidirectional") {
      return tx.exec_prepared("insert_edge_bi_unique", key[0].value, key[1].value, values[0].value, values[1].value);
    } else {
      throw std::invalid_argument("Received unknown type");
    }
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                               const std::vector<std::vector<Field>> &values) {
  std::lock_guard<std::mutex> lock(mutex_);

  assert(table == "edges" || table == "objects");
  return table == "edges" ? BatchInsertEdges(table, keys, values)
                          : BatchInsertObjects(table, keys, values);

}

Status CrdbDB::BatchInsertEdges(const std::string &table, const std::vector<std::vector<Field>> &keys,
                                  const std::vector<std::vector<Field>> &values) {
  try {
    pqxx::nontransaction tx(*conn_);

    std::string query = "INSERT INTO edges (id1, id2, type, timestamp, value) VALUES ";
    bool is_first = true;

    for (int i = 0; i < keys.size(); i++) {
      assert(keys[i].size() == 3);
      assert(keys[i][0].name == "id1");
      assert(keys[i][1].name == "id2");
      assert(keys[i][2].name == "type");
      assert(values[i].size() == 2);
      assert(values[i][0].name == "timestamp");
      assert(values[i][1].name == "value");

      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + conn_->quote(keys[i][0].value)         // id1
                + ", " + conn_->quote(keys[i][1].value)     // id2
                + ", " + conn_->quote(keys[i][2].value)     // type
                + ", " + values[i][0].value                 // timestamp
                + ", " + conn_->quote(values[i][1].value)   // value
              + ")";
    }

    pqxx::result queryRes = tx.exec(query);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::BatchInsertObjects(const std::string &table, const std::vector<std::vector<Field>> &keys,
                                const std::vector<std::vector<Field>> &values) {
  try {
    pqxx::nontransaction tx(*conn_);

    std::string query = "INSERT INTO objects (id, timestamp, value) VALUES ";
    bool is_first = true;

    for (int i = 0; i < keys.size(); i++) {
      assert(keys[i].size() == 1);
      assert(keys[i][0].name == "id");
      assert(values[i].size() == 2);
      assert(values[i][0].name == "timestamp");
      assert(values[i][1].name == "value");

      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + conn_->quote(keys[i][0].value)         // id
                + ", " + values[i][0].value                 // timestamp
                + ", " + conn_->quote(values[i][1].value)   // value
              + ")";
    }

    pqxx::result queryRes = tx.exec(query);
    
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::BatchRead(const std::string &table, int start_index, int end_index,
                         std::vector<std::vector<Field>> &result) {
  assert(table == "edges");
  assert(end_index > start_index && start_index >= 0);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = tx.exec_prepared("batch_read", end_index - start_index, start_index);

    std::vector<std::string> fields;
    fields.push_back("id1");
    fields.push_back("id2");
    fields.push_back("type");
    for (auto row : queryRes) {
      std::vector<Field> oneRowVector;
      for (int j = 0; j < fields.size(); j++) {
        Field f(fields[j], (row)[j].as<std::string>("NULL"));
        // f.name = (*fields)[j];
        // f.value = (row)[j].as<std::string>();
        oneRowVector.push_back(f);
      }
      result.push_back(oneRowVector);
    }

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }


}

Status CrdbDB::Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoDelete(tx, table, key, values);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoDelete(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == object_table_) {
    return tx.exec_prepared("delete_object", key[0].value, values[0].value);
  } else if (table == edge_table_) {
    return tx.exec_prepared("delete_edge", key[0].value, key[1].value, key[2].value, values[0].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Execute(const DB_Operation &operation, std::vector<std::vector<Field>> &result, bool txn_op) {
  try {
    switch (operation.operation) {
    case Operation::READ: {
      result.emplace_back();
      std::vector<std::string> read_fields;
      for (auto &field : operation.fields) {
        read_fields.push_back(field.name);
      }
      Status blah = Read(operation.table, operation.key, &read_fields, result.back());
      return blah;
    }
    break;
    case Operation::INSERT: {
      return Insert(operation.table, operation.key, operation.fields);
    }
    break;
    case Operation::UPDATE: {
      return Update(operation.table, operation.key, operation.fields);
    }
    break;
    case Operation::SCAN: {
      std::vector<std::string> scan_fields;
      for (auto &field : operation.fields) {
        scan_fields.push_back(field.name);
      }
      return Scan(operation.table, operation.key, operation.len, &scan_fields, result);
    }
    break;
    case Operation::READMODIFYWRITE: {
      return Status::kNotImplemented;
    }
    break;
    case Operation::DELETE: {
      return Delete(operation.table, operation.key, operation.fields);
    }
    break;
    case Operation:: MAXOPTYPE: {
      return Status::kNotImplemented;
    }
    break;
    default:
      return Status::kNotFound;
    }
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::ExecuteTransaction(const std::vector<DB_Operation> &operations, std::vector<std::vector<Field>> &results, bool read_only) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::work tx(*conn_);
    
    for (const auto &operation : operations) {
      pqxx::result queryRes;
      switch (operation.operation) {
      case Operation::READ: {
        std::vector<std::string> read_fields;
        for (auto &field : operation.fields) {
          read_fields.push_back(field.name);
        }
        queryRes = DoRead(tx, operation.table, operation.key, &read_fields);
      }
      break;
      case Operation::INSERT: {
        queryRes = DoInsert(tx, operation.table, operation.key, operation.fields);
      }
      break;
      case Operation::UPDATE: {
        queryRes = DoUpdate(tx, operation.table, operation.key, operation.fields);
      }
      break;
      case Operation::SCAN: {
        std::vector<std::string> scan_fields;
        for (auto &field : operation.fields) {
          scan_fields.push_back(field.name);
        }
        queryRes = DoScan(tx, operation.table, operation.key, operation.len, &scan_fields);
      }
      break;
      case Operation::READMODIFYWRITE: {
        return Status::kNotImplemented;
      }
      break;
      case Operation::DELETE: {
        queryRes = DoDelete(tx, operation.table, operation.key, operation.fields);
      }
      break;
      case Operation:: MAXOPTYPE: {
        return Status::kNotImplemented;
      }
      break;
      default:
        return Status::kNotFound;
      }

      if (operation.operation == Operation::READ || operation.operation == Operation::SCAN) {
        for (auto row : queryRes) {
          std::vector<Field> oneRowVector;
          for (int j = 0; j < operation.fields.size(); j++) {
            Field f(operation.fields[j].name, (row)[j].as<std::string>("NULL"));
            oneRowVector.push_back(f);
          }
          results.push_back(oneRowVector);
        }
      }
    }

    tx.commit();
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
  

}

DB *NewCrdbDB() {
  return new CrdbDB;
}

const bool registered = DBFactory::RegisterDB("crdb", NewCrdbDB);

} // benchmark
