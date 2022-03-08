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

  // scan (not yet implemented)
  // conn_->prepare("scan_object", "SELECT id FROM " + object_table_ + " WHERE id > $1 AND id < $2 ORDER BY id LIMIT $3");
  // conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE (id1, id2, type) > ($1, $2, $3) AND (id1, id2, type) < ($4, $5, $6) ORDER BY id1, id2, type LIMIT $7");

  // update
  // conn_->prepare("scan_object", "SELECT id FROM " + object_table_ + " WHERE id > $1 ORDER BY id LIMIT $2");
  // conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE (id1, id2, type) > ($1, $2, $3) ORDER BY id1, id2, type LIMIT $4");
  conn_->prepare("update_object", "UPDATE " + object_table_ + " SET timestamp = $1, value = $2 WHERE id = $3 AND timestamp < $1");
  conn_->prepare("update_edge", "UPDATE " + edge_table_ + " SET timestamp = $1, value = $2 WHERE id1 = $3 AND id2 = $4 AND type = $5 AND timestamp < $1");

  // Insert
  // type: unique = 0, bidirectional = 1, unique_and_bidirectional = 2, other = 3
  conn_->prepare("insert_object", "INSERT INTO " +object_table_ + " (id, timestamp, value) VALUES ($1, $2, $3)");

  std::string insert_edge = "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, $3, $4, $5 WHERE NOT EXISTS ";
  conn_->prepare("insert_edge_other", insert_edge + "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND type=0) OR (id1=$1 AND type=2) OR (id1=$1 AND id2=$2 AND type=1) OR (id1=$2 AND id2=$1))");
  conn_->prepare("insert_edge_bidirectional", insert_edge + "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND type=0) OR (id1=$1 AND type=2) OR (id1=$1 AND id2=$2 AND type=3) OR (id1=$2 AND id2=$1 AND type=3) OR (id1=$1 AND id2=$2 AND type=0))");
  conn_->prepare("insert_edge_unique", insert_edge + "(SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 OR (id1=$2 AND id2=$1))");
  conn_->prepare("insert_edge_bi_unique", insert_edge + "(SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 OR (id1=$2 AND id2=$1 AND type=3) OR (id1=$2 AND id2=$1 AND type=0))");
  
  // deletes
  conn_->prepare("delete_object", "DELETE FROM " + object_table_ + " WHERE id = $1 AND timestamp < $2");
  conn_->prepare("delete_edge", "DELETE FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3 AND timestamp < $4");
  
  // batch read
  conn_->prepare("batch_read", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE ((id1, id2) = ($1, $2) AND type > $3 OR id1 = $1 AND id2 > $2 OR id1 > $1) AND (id1 < $4 OR id1 = $4 AND id2 < $5 OR (id1, id2) = ($4, $5) AND type < $6) LIMIT $7");
}

void CrdbDB::Cleanup() {
  conn_->close();
  delete conn_;
}

/*
  key always in the order {id1, id2, type} or {id1}
  fields always in the other {timestamp, value}
*/
Status CrdbDB::Read(DataTable table, const std::vector<Field> & key, std::vector<TimestampValue> &result) {

  std::lock_guard<std::mutex> lock(mutex_);

  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoRead(tx, table, key);

    // for (int i = 0; i < fields->size(); i++) {
    //   // Field f((*fields)[i], (queryRes[0][i]).as<std::string>("NULL"));
    //   // result.emplace_back(f);
    // }
    result.emplace_back( (queryRes[0][0]).as<int64_t>(0), (queryRes[0][1]).as<std::string>("NULL") );


    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoRead(pqxx::transaction_base &tx, const DataTable table, const std::vector<Field> &key) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("read_object", key[0].value);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("read_edge", key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Scan(DataTable table, const std::vector<Field> & key, int n, std::vector<TimestampValue> &buffer) {
  return Status::kNotImplemented;
}

// Status CrdbDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
//                          const std::vector<std::string> *fields,
//                          std::vector<std::vector<Field>> &result) {
//   assert(fields == nullptr);
//   std::vector<std::string> hardcoded_fields{"id1", "id2", "type"};
//   if (fields == nullptr) {
//     fields = &hardcoded_fields;
//   }

//   std::lock_guard<std::mutex> lock(mutex_);
//   try {
//     pqxx::nontransaction tx(*conn_);

//     pqxx::result queryRes = DoScan(tx, table, key, len, fields);

//     for (auto row : queryRes) {
//       std::vector<Field> oneRowVector;
//       for (int j = 0; j < fields->size(); j++) {
//         Field f((*fields)[j], (row)[j].as<std::string>("NULL"));
//         oneRowVector.push_back(f);
//       }
//       result.push_back(oneRowVector);
//     }

//     return Status::kOK;
//   } catch (std::exception const &e) {
//     std::cerr << e.what() << endl;
//     return Status::kError;
//   }
// }

// pqxx::result CrdbDB::DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int record_count,
//                          const std::vector<std::string> *fields, const std::vector<Field> &limit) {
//   // conn_->prepare("scan_object", "SELECT id FROM " + object_table_ + " WHERE id > $1 AND id < $2 ORDER BY id LIMIT $3");
//   // conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE (id1, id2, type) > ($1, $2, $3) AND (id1, id2, type) < ($4, $5, $6) ORDER BY id1, id2, type LIMIT $7");
//   if (table == object_table_) {
//     return tx.exec_prepared("scan_object", key[0].value, limit[0].value, record_count);
//   } else if (table == edge_table_) {
//     return tx.exec_prepared("scan_edge", key[0].value, key[1].value, key[2].value, limit[0].value, limit[1].value, limit[2].value, record_count);
//   } else {
//     throw std::invalid_argument("Received unknown table");
//   }
// }

Status CrdbDB::Update(DataTable table, const std::vector<DB::Field> &key, TimestampValue const &value)  {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoUpdate(tx, table, key, value);
    
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoUpdate(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, TimestampValue const &value) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("update_object", value.timestamp, value.value, key[0].value);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("update_edge", value.timestamp, value.value, key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Insert(DataTable table, const std::vector<Field> &key, const TimestampValue & value) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoInsert(tx, table, key, value);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoInsert(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue & value) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("insert_object", key[0].value, value.timestamp, value.value);
  } else if (table == DataTable::Edges) {
    benchmark::EdgeType type = static_cast<benchmark::EdgeType>(key[2].value);
    if (type == benchmark::EdgeType::Other) {
      return tx.exec_prepared("insert_edge_other", key[0].value, key[1].value, key[2].value, value.timestamp, value.value);
    } else if (type == benchmark::EdgeType::Bidirectional) {
      return tx.exec_prepared("insert_edge_bidirectional", key[0].value, key[1].value, key[2].value, value.timestamp, value.value);
    } else if (type == benchmark::EdgeType::Unique) {
      return tx.exec_prepared("insert_edge_unique", key[0].value, key[1].value, key[2].value, value.timestamp, value.value);
    } else if (type == benchmark::EdgeType::UniqueAndBidirectional) {
      return tx.exec_prepared("insert_edge_bi_unique", key[0].value, key[1].value, key[2].value, value.timestamp, value.value);
    } else {
      throw std::invalid_argument("Received unknown type");
    }
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::BatchInsert(DataTable table, const std::vector<std::vector<Field>> &keys,
                               const std::vector<TimestampValue> &values) {
  std::lock_guard<std::mutex> lock(mutex_);

  return table == DataTable::Edges ? BatchInsertEdges(table, keys, values)
                                   : BatchInsertObjects(table, keys, values);

}

Status CrdbDB::BatchInsertEdges(DataTable table, const std::vector<std::vector<Field>> &keys,
                                  const std::vector<TimestampValue> &values) {
  try {
    pqxx::nontransaction tx(*conn_);

    std::string query = "INSERT INTO edges (id1, id2, type, timestamp, value) VALUES ";
    bool is_first = true;

    for (int i = 0; i < keys.size(); i++) {
      assert(keys[i].size() == 3);
      assert(keys[i][0].name == "id1");
      assert(keys[i][1].name == "id2");
      assert(keys[i][2].name == "type");

      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + std::to_string(keys[i][0].value)         // id1
                + ", " + std::to_string(keys[i][1].value)     // id2
                + ", " + std::to_string(keys[i][2].value)     // type
                + ", " + std::to_string(values[i].timestamp)  // timestamp
                + ", " + conn_->quote(values[i].value)        // value
              + ")";
    }

    pqxx::result queryRes = tx.exec(query);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::BatchInsertObjects(DataTable table, const std::vector<std::vector<Field>> &keys,
                                  const std::vector<TimestampValue> &values) {
  try {
    pqxx::nontransaction tx(*conn_);

    std::string query = "INSERT INTO objects (id, timestamp, value) VALUES ";
    bool is_first = true;

    for (int i = 0; i < keys.size(); i++) {
      assert(keys[i].size() == 1);
      assert(keys[i][0].name == "id");

      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + std::to_string(keys[i][0].value)               // id
                + ", " + std::to_string(values[i].timestamp)        // timestamp
                + ", " + conn_->quote(values[i].value)              // value
              + ")";
    }

    pqxx::result queryRes = tx.exec(query);
    
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::BatchRead(DataTable table,  
                         std::vector<Field> const & floor_key,
                         std::vector<Field> const & ceil_key,
                         int n,
                         std::vector<std::vector<Field>> &result) {
  assert(floor_key.size() == 3);
  assert(floor_key[0].name == "id1");
  assert(floor_key[1].name == "id2");
  assert(floor_key[2].name == "type");
  assert(ceil_key.size() == 3);
  assert(ceil_key[0].name == "id1");
  assert(ceil_key[1].name == "id2");
  assert(ceil_key[2].name == "type");
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = tx.exec_prepared("batch_read", floor_key[0].value, floor_key[1].value, floor_key[2].value, ceil_key[0].value, ceil_key[1].value, ceil_key[2].value, n);

    for (auto row : queryRes) {
      std::vector<Field> row_result = {{"id1", (row)[0].as<int64_t>(0)},
                                       {"id2", (row)[1].as<int64_t>(0)},
                                       {"type", (row)[2].as<int64_t>(0)}};
      result.push_back(row_result);
    }

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }


}

Status CrdbDB::Delete(DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::nontransaction tx(*conn_);

    pqxx::result queryRes = DoDelete(tx, table, key, value);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

pqxx::result CrdbDB::DoDelete(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue &value) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("delete_object", key[0].value, value.timestamp);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("delete_edge", key[0].value, key[1].value, key[2].value, value.timestamp);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status CrdbDB::Execute(const DB_Operation &operation, std::vector<TimestampValue> &result, bool txn_op) {
  try {
    switch (operation.operation) {
    case Operation::READ: {
      return Read(operation.table, operation.key, result);
    }
    break;
    case Operation::INSERT: {
      return Insert(operation.table, operation.key, operation.time_and_value);
    }
    break;
    case Operation::UPDATE: {
      return Update(operation.table, operation.key, operation.time_and_value);
    }
    break;
    case Operation::SCAN: {
      return Status::kNotImplemented;
    }
    break;
    case Operation::READMODIFYWRITE: {
      return Status::kNotImplemented;
    }
    break;
    case Operation::DELETE: {
      return Delete(operation.table, operation.key, operation.time_and_value);
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

/*
* Method executes each operation within a transaction as a prepared statement
*/
Status CrdbDB::ExecuteTransactionPrepared(const std::vector<DB_Operation> &operations, std::vector<TimestampValue> &results, bool read_only) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::work tx(*conn_);
    
    for (const auto &operation : operations) {
      pqxx::result queryRes;
      switch (operation.operation) {
      case Operation::READ: {
        queryRes = DoRead(tx, operation.table, operation.key);
      }
      break;
      case Operation::INSERT: {
        queryRes = DoInsert(tx, operation.table, operation.key, operation.time_and_value);
      }
      break;
      case Operation::UPDATE: {
        queryRes = DoUpdate(tx, operation.table, operation.key, operation.time_and_value);
      }
      break;
      case Operation::SCAN: {
        return Status::kNotImplemented;
      }
      break;
      case Operation::READMODIFYWRITE: {
        return Status::kNotImplemented;
      }
      break;
      case Operation::DELETE: {
        queryRes = DoDelete(tx, operation.table, operation.key, operation.time_and_value);
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
          results.emplace_back( (row[0]).as<int64_t>(0), (row[1]).as<std::string>("NULL") );
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


/*
* Method batches together statements through semicolon appending and executes it as a plain, not prepared SQL statements
* Execution order (first to last): reads, scans, inserts, updates, deletes
*/
Status CrdbDB::ExecuteTransactionBatch(const std::vector<DB_Operation> &operations, std::vector<TimestampValue> &results, bool read_only) {
  std::string executionMethod = "plain";

  std::lock_guard<std::mutex> lock(mutex_);
  try {
    pqxx::work tx(*conn_);
    std::vector<DB_Operation> read_operations;
    std::vector<DB_Operation> insert_operations;
    std::vector<DB_Operation> update_operations;
    std::vector<DB_Operation> delete_operations;
    
    for (const auto &operation : operations) {
      switch (operation.operation) {
      case Operation::READ: {
        read_operations.push_back(operation);
      }
      break;
      case Operation::INSERT: {
        insert_operations.push_back(operation);
      }
      break;
      case Operation::UPDATE: {
        update_operations.push_back(operation);
      }
      break;
      case Operation::SCAN: {
        return Status::kNotImplemented;
      }
      break;
      case Operation::READMODIFYWRITE: {
        return Status::kNotImplemented;
      }
      break;
      case Operation::DELETE: {
        delete_operations.push_back(operation);
      }
      break;
      case Operation:: MAXOPTYPE: {
        return Status::kNotImplemented;
      }
      break;
      default:
        return Status::kNotFound;
      }
    }

    pqxx::result queryRes;

    // reads
    std::string read_query = GenerateMergedReadQuery(read_operations);
    if (executionMethod == "plain") {
      queryRes = tx.exec(read_query);
      for (auto row : queryRes) {
        results.emplace_back((row[0]).as<int64_t>(0), (row[1]).as<std::string>("NULL"));
      }
    }
    // else if (executionMethod == "stream") {
    //   // UNSURE IF THIS WILL WORK BECAUSE STREAM_FROM USES copy to AND CRDB DOES NOT SUPPORT copy to
    //   auto stream = tx.stream(read_query);
    //   std::tuple<std::string, std::string> row;
    //   while (stream >> row) {
    //     std::vector<Field> oneRowVector;
    //     for (int j = 0; j < operation.fields.size(); j++) {
    //       Field f(operation.fields[j].name, std::get<j>(row));
    //       oneRowVector.push_back(f);
    //     }
    //     results.push_back(oneRowVector);
    //   }
    // }
    
    // inserts
    if (executionMethod == "plain") {
      std::string insert_query = GenerateMergedInsertQuery(insert_operations);
      queryRes = tx.exec(insert_query);
    }
    // else if (executionMethod == "stream") {
    //   std::vector<DB_Operation> edge_inserts;
    //   std::vector<DB_Operation> object_inserts;
    //   for (const DB_Operation &operation : insert_operations) {
    //     if operation.
    //   }
    //   pqxx::stream_to stream{tx, }
    // }

    // updates
    if (executionMethod == "plain") {
      std::string update_query = GenerateMergedUpdateQuery(update_operations);
      queryRes = tx.exec(update_query);
    }

    // deletes
    if (executionMethod == "plain") {
      std::string delete_query = GenerateMergedDeleteQuery(delete_operations);
      queryRes = tx.exec(delete_query);
    }
    
    tx.commit();
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << endl;
    return Status::kError;
  }
}

Status CrdbDB::ExecuteTransaction(const std::vector<DB_Operation> &operations, std::vector<TimestampValue> &results, bool read_only) {
  // TODO: replace this hardcoded method with a parameter
  std::string executionMethod = "batch";

  if (executionMethod == "prepared") {
    return ExecuteTransactionPrepared(operations, results, read_only);
  } else if (executionMethod == "batch") {
    return ExecuteTransactionBatch(operations, results, read_only);
  } else {
    cout << "Attempted to perform CRDB ExecuteTransaction with unsupported execution method: " << executionMethod << endl;
    return Status::kNotImplemented;
  }
}

std::string CrdbDB::GenerateMergedReadQuery(const std::vector<DB_Operation> &read_operations) {
  std::string query = "";
  for (int i = 0; i < read_operations.size(); i++) {
    const DB_Operation operation = read_operations[i];
    if (operation.table == DataTable::Objects) {
      query += "SELECT timestamp, value FROM " + object_table_ + " WHERE id = " + std::to_string((operation.key)[0].value);
    } else if (operation.table == DataTable::Edges) {
      query += "SELECT timestamp, value FROM " + edge_table_ + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value);
    }
    query += ";";
  }

  return query;
}

std::string CrdbDB::GenerateMergedInsertQuery(const std::vector<DB_Operation> &insert_operations) {
  std::string query = "";
  for (int i = 0; i < insert_operations.size(); i++) {
    const DB_Operation operation = insert_operations[i];
    if (operation.table == DataTable::Objects) {
      query += "INSERT INTO " +object_table_ + " (id, timestamp, value) VALUES (" + std::to_string((operation.key)[0].value) + ", " + std::to_string(operation.time_and_value.timestamp) + ", " + conn_->quote(operation.time_and_value.value) + ")";
    } else if (operation.table == DataTable::Edges) {
      std::string id1 = std::to_string((operation.key)[0].value);
      std::string id2 = std::to_string((operation.key)[1].value);
      std::string type = std::to_string((operation.key)[2].value);
      std::string timestamp = std::to_string(operation.time_and_value.timestamp);
      std::string value = conn_->quote(operation.time_and_value.value);
      benchmark::EdgeType edge_type = static_cast<benchmark::EdgeType>((operation.key)[2].value);
      query += "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT " + id1 + ", " + id2 + ", " + type + ", " + timestamp + ", " + value + " WHERE NOT EXISTS ";
      if (edge_type == benchmark::EdgeType::Other) {
        query +=  "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND type=0) OR (id1=" + id1 + " AND type=2) OR (id1=" + id1 + " AND id2=" + id2 + " AND type=1) OR (id1=" + id2 + " AND id2=" + id1 + "))";
      } else if (edge_type == benchmark::EdgeType::Bidirectional) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND type=0) OR (id1=" + id1 + " AND type=2) OR (id1=" + id1 + " AND id2=" + id2 + " AND type=3) OR (id1=" + id2 + " AND id2=" + id1 + " AND type=3) OR (id1=" + id1 + " AND id2=" + id2 + " AND type=0))";
      } else if (edge_type == benchmark::EdgeType::Unique) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE id1=" + id1 + " OR (id1=" + id2 + " AND id2=" + id1 + "))";
      } else if (edge_type == benchmark::EdgeType::UniqueAndBidirectional) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE id1=" + id1 + " OR (id1=" + id2 + " AND id2=" + id1 + " AND type=3) OR (id1=" + id2 + " AND id2=" + id1 + " AND type=0))";
      }
    }
    query += ";";
  }

  return query;

  // queryRes = DoInsert(tx, operation.table, operation.key, operation.fields);

  // pqxx::result CrdbDB::DoInsert(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  //   if (table == object_table_) {
  //     return tx.exec_prepared("insert_object", key[0].value, values[0].value, values[1].value);
  //   } else if (table == edge_table_) {
  //     std::string type = key[2].value;
  //     if (type == "other") {
  //       return tx.exec_prepared("insert_edge_other", key[0].value, key[1].value, values[0].value, values[1].value);
  //     } else if (type == "bidirectional") {
  //       return tx.exec_prepared("insert_edge_bidirectional", key[0].value, key[1].value, values[0].value, values[1].value);
  //     } else if (type == "unique") {
  //       return tx.exec_prepared("insert_edge_unique", key[0].value, key[1].value, values[0].value, values[1].value);
  //     } else if (type == "unique_and_bidirectional") {
  //       return tx.exec_prepared("insert_edge_bi_unique", key[0].value, key[1].value, values[0].value, values[1].value);
  //     } else {
  //       throw std::invalid_argument("Received unknown type");
  //     }
  //   } else {
  //     throw std::invalid_argument("Received unknown table");
  //   }
  // }

  // conn_->prepare("insert_object", "INSERT INTO " + object_table_ + " (id, timestamp, value) SELECT $1, $2, $3");
  // conn_->prepare("insert_edge_other", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'other', $3, $4 WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1 AND type = 'unique') OR (id1 = $1 AND type='unique_and_bidirectional') OR (id1 = $1 AND id2 = $2 AND type = 'bidirectional') OR (id1 = $2 AND id2 = $1))");
  // conn_->prepare("insert_edge_bidirectional", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'bidirectional', $3, $4 WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1 AND type = 'unique') OR (id1 = $1 AND type='unique_and_bidirectional') OR (id1 = $1 AND id2 = $2 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'unique'))");
  // conn_->prepare("insert_edge_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'unique', $3, $4 WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1) OR (id1 = $2 AND id2 = $1))");
  // conn_->prepare("insert_edge_bi_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, 'unique_and_bidirectional', $3, $4 WHERE NOT EXISTS (SELECT id1 AS id1, id2 AS id2, type AS type FROM " + edge_table_ +" WHERE (id1 = $1) OR (id1 = $2 AND id2 = $1 AND type = 'other') OR (id1 = $2 AND id2 = $1 AND type = 'unique'))");
}

std::string CrdbDB::GenerateMergedUpdateQuery(const std::vector<DB_Operation> &update_operations) {
  std::string query = "";
  for (int i = 0; i < update_operations.size(); i++) {
    const DB_Operation operation = update_operations[i];
    if (operation.table == DataTable::Objects) {
      query += "UPDATE " + object_table_ + " SET timestamp = " + std::to_string(operation.time_and_value.timestamp) + ", value = " + conn_->quote(operation.time_and_value.value) + " WHERE id = " + std::to_string((operation.key)[0].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp);
    } else if (operation.table == DataTable::Edges) {
      query += "UPDATE " + edge_table_ + " SET timestamp = " + std::to_string(operation.time_and_value.timestamp) + ", value = " + conn_->quote(operation.time_and_value.value) + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp);
    }
    query += ";";
  }

  return query;


  // queryRes = DoUpdate(tx, operation.table, operation.key, operation.fields);

  // if (table == object_table_) {
  //   return tx.exec_prepared("update_object", values[0].value, values[1].value, key[0].value);
  // } else if (table == edge_table_) {
  //   return tx.exec_prepared("update_edge", values[0].value, values[1].value, key[0].value, key[1].value, key[2].value);
  // } else {
  //   throw std::invalid_argument("Received unknown table");
  // }

  // conn_->prepare("update_object", "UPDATE " + object_table_ + " SET timestamp = $1, value = $2 WHERE id = $3 AND timestamp < $1");
  // conn_->prepare("update_edge", "UPDATE " + edge_table_ + " SET timestamp = $1, value = $2 WHERE id1 = $3 AND id2 = $4 AND type = $5 AND timestamp < $1");
}

std::string CrdbDB::GenerateMergedDeleteQuery(const std::vector<DB_Operation> &delete_operations) {
  std::string query = "";
  for (int i = 0; i < delete_operations.size(); i++) {
    const DB_Operation operation = delete_operations[i];
    if (operation.table == DataTable::Objects) {
      query += "DELETE FROM " + object_table_ + " WHERE id = " + std::to_string((operation.key)[0].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp);
    } else if (operation.table == DataTable::Edges) {
      query += "DELETE FROM " + edge_table_ + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp);
    }
    query += ";";
  }

  return query;

  // queryRes = DoDelete(tx, operation.table, operation.key, operation.fields);

  // pqxx::result CrdbDB::DoDelete(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  //   if (table == object_table_) {
  //     return tx.exec_prepared("delete_object", key[0].value, values[0].value);
  //   } else if (table == edge_table_) {
  //     return tx.exec_prepared("delete_edge", key[0].value, key[1].value, key[2].value, values[0].value);
  //   } else {
  //     throw std::invalid_argument("Received unknown table");
  //   }
  // }

  // conn_->prepare("delete_object", "DELETE FROM " + object_table_ + " WHERE id = $1 AND timestamp < $2");
  // conn_->prepare("delete_edge", "DELETE FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3 AND timestamp < $4");
}

DB *NewCrdbDB() {
  return new CrdbDB;
}

const bool registered = DBFactory::RegisterDB("crdb", NewCrdbDB);

} // benchmark
