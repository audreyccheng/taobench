#include "ybsql_db.h"
#include "db_factory.h"
#include <pqxx/pqxx>
#include "pqxx/nontransaction"
#include <chrono>
#include <pqxx/except>

namespace {
    const std::string DATABASE_STRING = "ybsql_db.string";

    inline bool IsContentionMessage(std::string const & msg) {
      return msg.find("aborted") != std::string::npos || msg.find("Restart read required") != std::string::npos || msg.find("Try again") != std::string::npos;
  }
};

namespace benchmark {

void YsqlDB::Init() {
    std::lock_guard<std::mutex> lock(mu_);

    // Get string from "ybsql_db.properties"
    const utils::Properties &props = *props_;
    std::string str = props.GetProperty(DATABASE_STRING);
    // Start Connection
    ysql_conn_ = new pqxx::connection(str);

    // Prepare statements 
    edge_table_ = props_->GetProperty("edge_table_", "edges1");
    object_table_ = props_->GetProperty("object_table_", "objects1");

    // Read
    ysql_conn_->prepare("read_object", "SELECT timestamp, value FROM " +object_table_ + " WHERE id = $1");
    ysql_conn_->prepare("read_edge", "SELECT timestamp, value FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3");

    // Scan
    // ysql_conn_->prepare("scan_object", "SELECT id FROM " +object_table_ + " WHERE yb_hash_code(id) > $1 AND yb_hash_code(id) < $2");
    // ysql_conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE yb_hash_code(id1) > $1 AND yb_hash_code(id1) < $2");

    // Update
    ysql_conn_->prepare("update_object", "UPDATE " +object_table_ + " SET timestamp = $1, value = $2 WHERE id = $3 AND timestamp < $1");
    ysql_conn_->prepare("update_edge", "UPDATE " + edge_table_ + " SET timestamp = $1, value = $2 WHERE id1 = $3 AND id2 = $4 AND type = $5 AND timestamp < $1");

    // Insert
    // type: unique = 0, bidirectional = 1, unique_and_bidirectional = 2, other = 3
    std::string insert_edge = "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1, $2, $3, $4, $5 WHERE NOT EXISTS ";
    ysql_conn_->prepare("insert_object", "INSERT INTO " +object_table_ + " (id, timestamp, value) SELECT $1, $2, $3");
    //ysql_conn_->prepare("insert_edge_other", insert_edge + " (SELECT 1 FROM " + edge_table_  + " WHERE (id1=$1 AND type=0) OR (id1=$1 AND type=2) OR (id1=$1 AND id2=$2 AND type=1) OR (id1=$2 AND id2=$1))");
    //ysql_conn_->prepare("insert_edge_bidirectional", insert_edge + " WHERE (id1=$1 AND type=0) OR (id1=$1 AND type=2) OR (id1=$1 AND id2=$2 AND type=3) OR (id1=$2 AND id2=$1 AND type=3) OR (id1=$1 AND id2=$2 AND type=0))");
    ysql_conn_->prepare("insert_edge_other", insert_edge + 
    " (SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND type IN (0, 2)) UNION ALL" + 
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND id2=$2 AND type=1) UNION ALL" +  
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$2 AND id2=$1))");

    ysql_conn_->prepare("insert_edge_bidirectional", insert_edge + 
    " (SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND type IN (0, 2)) UNION ALL" + 
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$1 AND id2=$2 AND type IN (0, 3)) UNION ALL" +  
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$2 AND id2=$1 AND type=3))");

    ysql_conn_->prepare("insert_edge_unique", insert_edge + 
    " (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 UNION ALL" +
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$2 AND id2=$1))");

    ysql_conn_->prepare("insert_edge_bi_unique", insert_edge + 
    " (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 UNION ALL" +
    " SELECT 1 FROM " + edge_table_ + " WHERE (id1=$2 AND id2=$1 AND type IN (0, 3)))");

    // Delete
    ysql_conn_->prepare("delete_object", "DELETE FROM " +object_table_ + " WHERE id = $1 AND timestamp < $2");
    ysql_conn_->prepare("delete_edge", "DELETE FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3 AND timestamp < $4");
    // Batch Read
    ysql_conn_->prepare("batch_read", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE ((id1, id2, type) > ($1, $2, $3) AND (id1, id2, type) < ($4, $5, $6)) ORDER BY id1, id2, type LIMIT $7");
    // ysql_conn_->prepare("batch_read", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE yb_hash_code(id1, id2, type) > yb_hash_code($1::bigint, $2::bigint, $3::smallint) AND yb_hash_code(id1, id2, type) < yb_hash_code($4::bigint, $5::bigint, $6::smallint) LIMIT $7");
    // ysql_conn_->prepare("batch_read", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE yb_hash_code(id1, id2, type) >= $1 AND yb_hash_code(id1, id2, type) <= $2 LIMIT $3");
}

void YsqlDB::Cleanup() {
  ysql_conn_->close();
  delete ysql_conn_;
}

Status YsqlDB::Read(DataTable table, const std::vector<Field> &key, std::vector<TimestampValue> &result) {

    const std::lock_guard<std::mutex> lock(mu_);
    // Execute SQL commands
    try {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoRead(tx, table, key);

      // for (size_t i = 0; i < fields->size(); i++) {
      //   Field f((*fields)[i], (r[0][i]).as<std::string>("NULL"));
      //   result.emplace_back(f);
      // }
      result.emplace_back((r[0][0]).as<int64_t>(0), (r[0][1]).as<std::string>("NULL"));
      return Status::kOK;
    }
    catch (const std::exception &e) {
      std::cerr << "Read failed: " << e.what() << std::endl;
      return Status::kError;
    }
}

/* Helper function to execute the read prepare statement 
   Objects: key[0] = id
   Edges: key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoRead(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("read_object", key[0].value);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("read_edge", key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}


Status YsqlDB::Scan(DataTable table,
                    const std::vector<Field> &key,
                    int n,
                    std::vector<TimestampValue> &buffer) {
    // const std::lock_guard<std::mutex> lock(mu_);
    // assert(table == "edges");
    // assert(fields == NULL);
    // std::vector<std::string> hardcoded_fields{"id1", "id2", "type"};
    // if (fields == NULL) {
    //   fields = &hardcoded_fields;
    // }

    // try {
    //   pqxx::nontransaction tx(*ysql_conn_);
    //   pqxx::result r = DoScan(tx, table, key, len, fields, start, end);
    //   for (auto row : r) {
    //     std::vector<Field> rowVector;
    //     for (int j = 0; j < fields->size(); j++) {
    //       Field f((*fields)[j], (row)[j].as<std::string>("NULL"));
    //       rowVector.push_back(f);
    //     }
    //     result.push_back(rowVector);
    //   }
    //   return Status::kOK;
    // }
    // catch (const std::exception &e) {
    //   std::cerr << e.what() << std::endl;
    //   return Status::kError;
    // }
    return Status::kNotImplemented;
}

/* Helper function to execute the scan prepare statement 
   start: the start of the yb_hash_code(id) to scan
   end: the end of the yb_hash_code(id) to scan */
// pqxx::result YsqlDB::DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
//                          const std::vector<std::string> *fields, int start, int end) {
//   if (table == "objects") {
//     return tx.exec_prepared("scan_object", start, end, len);
//   } else if (table == "edges") {
//     return tx.exec_prepared("scan_edge", start, end, len);
//   } else {
//     throw std::invalid_argument("Received unknown table");
//   }
// }

Status YsqlDB::Update(DataTable table, const std::vector<DB::Field> &key, TimestampValue const &value) {
    
    const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoUpdate(tx, table, key, value);     
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      std::cerr << "Update failed: " << e.what() << std::endl;
      //return IsContentionMessage(e.what()) ? Status::kContentionError : Status::kError;
      return Status::kError;
    }
}

/* Helper function to execute the update prepare statement 
   Objects: key[0] = id
   Edges: key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoUpdate(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, TimestampValue const &timeval) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("update_object", timeval.timestamp, timeval.value, key[0].value);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("update_edge", timeval.timestamp, timeval.value, key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}


Status YsqlDB::Insert(DataTable table, const std::vector<Field> &key, const TimestampValue & timeval) {
    assert(!key.empty());
    const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoInsert(tx, table, key, timeval);      
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      std::cerr << "Insert failed: " << e.what() << std::endl;
      return IsContentionMessage(e.what()) ? Status::kContentionError : Status::kError;
      //return Status::kError;
    }
}

/* Helper function to execute the insert prepare statement 
   Objects: key[0] = id
   Edges: key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoInsert(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue & timeval) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("insert_object", key[0].value, timeval.timestamp, timeval.value);
  } else if (table == DataTable::Edges) {
    benchmark::EdgeType type = static_cast<benchmark::EdgeType>(key[2].value);
    if (type == benchmark::EdgeType::Other) {
      return tx.exec_prepared("insert_edge_other", key[0].value, key[1].value, key[2].value, timeval.timestamp, timeval.value);
    } else if (type == benchmark::EdgeType::Bidirectional) {
      return tx.exec_prepared("insert_edge_bidirectional", key[0].value, key[1].value, key[2].value, timeval.timestamp, timeval.value);
    } else if (type == benchmark::EdgeType::Unique) {
      return tx.exec_prepared("insert_edge_unique", key[0].value, key[1].value, key[2].value, timeval.timestamp, timeval.value);
    } else if (type == benchmark::EdgeType::UniqueAndBidirectional) {
      return tx.exec_prepared("insert_edge_bi_unique", key[0].value, key[1].value, key[2].value, timeval.timestamp, timeval.value);
    } else {
      throw std::invalid_argument("Received unknown type");
    }
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status YsqlDB::BatchInsert(DataTable table, const std::vector<std::vector<Field>> &keys,
                             const std::vector<TimestampValue> &timevals) {
    const std::lock_guard<std::mutex> lock(mu_);
    return table == DataTable::Edges 
        ? BatchInsertEdges(table, keys, timevals)
        : BatchInsertObjects(table, keys, timevals);
}

/* Helper function to do batch insert for objects */
Status YsqlDB::BatchInsertObjects(DataTable table,
                                   const std::vector<std::vector<Field>> &keys,
                                   const std::vector<TimestampValue> &timevals) {
  assert(!keys.empty());
  try {
    pqxx::nontransaction tx(*ysql_conn_);
    std::string query = "INSERT INTO " + object_table_ + " (id, timestamp, value) VALUES ";
    bool is_first = true;
    for (size_t i = 0; i < keys.size(); ++i) {
      assert(keys[i].size() == 1);
      assert(keys[i][0].name == "id");
      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + std::to_string(keys[i][0].value) +                   // id
                      ", " + std::to_string(timevals[i].timestamp) +         // timestamp
                      ", " + ysql_conn_->quote(timevals[i].value) + ")";    // value
    }
    pqxx::result r = tx.exec(query);      
    return Status::kOK;
  } catch (const std::exception &e) {
    std::cerr << "Batch insert object failed:" << e.what() << std::endl;
    return Status::kError;
  }
}

/* Helper function to do batch insert for edges */
Status YsqlDB::BatchInsertEdges(DataTable table,
                                 const std::vector<std::vector<Field>> &keys,
                                 const std::vector<TimestampValue> &timevals)
{
 try {
    pqxx::nontransaction tx(*ysql_conn_);

    std::string query = "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) VALUES ";
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
                + ", " + std::to_string(timevals[i].timestamp)   // timestamp
                + ", " + ysql_conn_->quote(timevals[i].value)   // value
              + ")";
    }
    pqxx::result queryRes = tx.exec(query);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}


Status YsqlDB::BatchRead(DataTable table,  
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
  //const std::lock_guard<std::mutex> lock(mu_);
  try {
    pqxx::nontransaction tx(*ysql_conn_);
    //std::cout << "SELECT id1, id2, type FROM " + edge_table_ + " WHERE ((id1, id2, type) > (" + std::to_string(floor_key[0].value) + ", " +  std::to_string(floor_key[1].value) + ", " + std::to_string(floor_key[2].value) + ") AND (id1, id2, type) < (" + std::to_string(ceil_key[0].value) + ", " + std::to_string(ceil_key[1].value) + ", " + std::to_string(ceil_key[2].value) + ")) LIMIT" + std::to_string(n) << std::endl;
    pqxx::result queryRes = tx.exec_prepared("batch_read", floor_key[0].value, floor_key[1].value, floor_key[2].value, ceil_key[0].value, ceil_key[1].value, ceil_key[2].value, n);
    //pqxx::result queryRes = tx.exec_prepared("batch_read", ysql_start, ysql_end, n);

    int rows_found = 0;
    for (auto row : queryRes) {
      std::vector<Field> row_result = {{"id1", (row)[0].as<int64_t>(0)},
        {"id2", (row)[1].as<int64_t>(0)},
        {"type", (row)[2].as<int64_t>(0)}};
      result.emplace_back(std::move(row_result));
      ++rows_found;
    }
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}   

Status YsqlDB::Delete(DataTable table, const std::vector<Field> &key, const TimestampValue & timeval) {

    //const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoDelete(tx, table, key, timeval);     
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      return IsContentionMessage(e.what()) ? Status::kContentionError : Status::kError;
      //return Status::kError;
    }
}


/* Helper function to execute the delete prepare statement 
   Objects: key[0] = id
   Edges: key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoDelete(pqxx::transaction_base &tx, DataTable table, const std::vector<Field> &key, const TimestampValue & timeval) {
  if (table == DataTable::Objects) {
    return tx.exec_prepared("delete_object", key[0].value, timeval.timestamp);
  } else if (table == DataTable::Edges) {
    return tx.exec_prepared("delete_edge", key[0].value, key[1].value, key[2].value, timeval.timestamp);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status YsqlDB::Execute(const DB_Operation &operation,
                       std::vector<TimestampValue> &result, bool tx_op) {
  try {
    switch (operation.operation) {
    case Operation::READ: {
      // result.emplace_back();
      // std::vector<std::string> read_fields;
      // for (auto &field : operation.fields) {
      //   read_fields.push_back(field.name);
      // }
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
      // std::vector<std::string> scan_fields;
      // for (auto &field : operation.fields) {
      //   scan_fields.push_back(field.name);
      // }
      // return Scan(operation.table, operation.key, operation.len, &scan_fields, result, 0, 1);
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
    std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}

Status YsqlDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                  std::vector<TimestampValue> &results, bool read_only) {                     
  std::string method = "batch"; // hardcoded

  if (method == "prepared") {
    return ExecuteTransactionPrepared(operations, results, read_only);
  } else if (method == "batch") {
    return ExecuteTransactionBatch(operations, results, read_only);
  } else {
    std::cout << "Unknow execute txn method" << std::endl;
    return Status::kError;
  }
}


Status YsqlDB::ExecuteTransactionPrepared(const std::vector<DB_Operation> &operations,
                                      std::vector<TimestampValue> &results, bool read_only) {                     
  try {
    const std::lock_guard<std::mutex> lock(mu_);
    pqxx::work tx(*ysql_conn_);
    
    for (const auto &operation : operations) {
      pqxx::result queryRes;
      switch (operation.operation) {
      case Operation::READ: {
        // std::vector<std::string> read_fields;
        // for (auto &field : operation.fields) {
        //   read_fields.push_back(field.name);
        // }
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
      // Not reached. We do not have scan inside transaction.
      case Operation::SCAN: {
        // std::vector<std::string> scan_fields;
        // for (auto &field : operation.fields) {
        //   scan_fields.push_back(field.name);
        // }
        // queryRes = DoScan(tx, operation.table, operation.key, operation.len, &scan_fields, 0, 4096);
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
          // std::vector<Field> oneRowVector;
          // for (int j = 0; j < operation.fields.size(); j++) {
          //   Field f(operation.fields[j].name, (row)[j].as<std::string>());
          //   oneRowVector.push_back(f);
          // }
          // results.push_back(oneRowVector);
          results.emplace_back((row[0]).as<int64_t>(0), (row[1]).as<std::string>("NULL"));
        }
      }
    }
    tx.commit();
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << "txn failed: " << e.what() << std::endl;
    return Status::kError;
  }
}

Status YsqlDB::ExecuteTransactionBatch(const std::vector<DB_Operation> &operations,
                                      std::vector<TimestampValue> &results, bool read_only) {    
  try {
    const std::lock_guard<std::mutex> lock(mu_);
    pqxx::work tx(*ysql_conn_);
    pqxx::result queryRes;

    // Group the operations by type
    std::vector<DB_Operation> read_object_ops;
    std::vector<DB_Operation> read_edges_ops;
    //std::vector<DB_Operation> scan_ops;
    std::vector<DB_Operation> insert_ops;
    std::vector<DB_Operation> update_ops;
    std::vector<DB_Operation> delete_ops;

    if (read_only) {
      for (const auto &operation : operations) {
        assert(operation.operation == Operation::READ);
        if (operation.table == DataTable::Edges) {
          read_edges_ops.emplace_back(operation);
        } else {
          read_object_ops.emplace_back(operation);
        }
      }
      if (!read_edges_ops.empty()) {
        std::string readEdgeQuery = ReadBatchQuery(read_edges_ops);
        queryRes = tx.exec(readEdgeQuery);
        for (auto row : queryRes) {
          results.emplace_back((row[0]).as<int64_t>(0), (row[1]).as<std::string>("NULL"));
        }
      }     
      if (!read_object_ops.empty()) {
        std::string readObjectQuery = ReadBatchQuery(read_object_ops);
        queryRes = tx.exec(readObjectQuery);
        for (auto row : queryRes) {
          results.emplace_back((row[0]).as<int64_t>(0), (row[1]).as<std::string>("NULL"));
        }
      }
    } else {
      for (const auto &operation : operations) {
          switch (operation.operation) {
          case Operation::INSERT: {
            insert_ops.emplace_back(operation);
          }
          break;
          case Operation::UPDATE: {
            update_ops.emplace_back(operation);
          }
          break;
          case Operation::DELETE: {
            delete_ops.emplace_back(operation);
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
          case Operation:: MAXOPTYPE: {
            return Status::kNotImplemented;
          }
          break;
          default:
            return Status::kNotFound;
          }
        }
        std::string insertQuery = InsertBatchQuery(insert_ops);
        std::string updateQuery = UpdateBatchQuery(update_ops);
        std::string deleteQuery = DeleteBatchQuery(delete_ops);

        queryRes = tx.exec(insertQuery);
        queryRes = tx.exec(updateQuery);
        queryRes = tx.exec(deleteQuery);
    }
    tx.commit();
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << "txn failed: " << e.what() << std::endl;
    return IsContentionMessage(e.what()) ? Status::kContentionError : Status::kError;
    //return Status::kError;
  }
}


std::string YsqlDB::ReadBatchQuery(const std::vector<DB_Operation> &read_ops) {
  std::string query = "";

  for (int i = 0; i < read_ops.size(); i++) {
    const DB_Operation operation = read_ops[i];
    if (operation.table == DataTable::Objects) {
      query += "SELECT timestamp, value FROM " + object_table_ + " WHERE id = " + std::to_string((operation.key)[0].value) + ";";
    } else if (operation.table == DataTable::Edges) {
      query += "SELECT timestamp, value FROM " + edge_table_ + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value) + ";";
    }
  }

  return query;
}

std::string YsqlDB::ScanBatchQuery(const std::vector<DB_Operation> &scan_ops) {
  std::string query = "";

  return query;
}

std::string YsqlDB::InsertBatchQuery(const std::vector<DB_Operation> &insert_ops) {
  std::string query = "";
  
   for (size_t i = 0; i < insert_ops.size(); i++) {
    const DB_Operation operation = insert_ops[i];
    if (operation.table == DataTable::Objects) {
      query += "INSERT INTO " +object_table_ + " (id, timestamp, value) VALUES (" + std::to_string((operation.key)[0].value) + ", " + std::to_string(operation.time_and_value.timestamp) + ", " + ysql_conn_->quote(operation.time_and_value.value) + ");";
    } else if (operation.table == DataTable::Edges) {
      std::string id1 = std::to_string((operation.key)[0].value);
      std::string id2 = std::to_string((operation.key)[1].value);
      std::string type = std::to_string((operation.key)[2].value);
      std::string timestamp = std::to_string(operation.time_and_value.timestamp);
      std::string value = ysql_conn_->quote(operation.time_and_value.value);
      benchmark::EdgeType edge_type = static_cast<benchmark::EdgeType>((operation.key)[2].value);
      query += "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT " + id1 + ", " + id2 + ", " + type + ", " + timestamp + ", " + value + " WHERE NOT EXISTS ";
      if (edge_type == benchmark::EdgeType::Other) {
        query +=  "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND type IN (0, 2)) UNION ALL " +
                  "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND id2=" + id2 + " AND type=1) UNION ALL " + 
                  "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id2 + " AND id2=" + id1 + "));";
      } else if (edge_type == benchmark::EdgeType::Bidirectional) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND type IN (0, 2)) UNION ALL " +
                 "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id1 + " AND id2=" + id2 + " AND type IN (0, 3)) UNION ALL " +
                 "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id2 + " AND id2=" + id1 + " AND type=3));";
      } else if (edge_type == benchmark::EdgeType::Unique) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE id1=" + id1 + " UNION ALL " +
                 "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id2 + " AND id2=" + id1 + "));";
      } else if (edge_type == benchmark::EdgeType::UniqueAndBidirectional) {
        query += "(SELECT 1 FROM " + edge_table_ + " WHERE id1=" + id1 + " UNION ALL " +
                 "SELECT 1 FROM " + edge_table_ + " WHERE (id1=" + id2 + " AND id2=" + id1 + " AND type IN (0, 3)));";
      }
    }
  }
  return query;
}

std::string YsqlDB::UpdateBatchQuery(const std::vector<DB_Operation> &update_ops) {
  std::string query = "";

   for (int i = 0; i < update_ops.size(); i++) {
    const DB_Operation operation = update_ops[i];
    if (operation.table == DataTable::Objects) {
      query += "UPDATE " +object_table_ + " SET timestamp = " + std::to_string(operation.time_and_value.timestamp) + ", value = " + ysql_conn_->quote(operation.time_and_value.value) + " WHERE id = " + std::to_string((operation.key)[0].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp) + ";";
    } else if (operation.table == DataTable::Edges) {
      query += "UPDATE " + edge_table_ + " SET timestamp = " + std::to_string(operation.time_and_value.timestamp) + ", value = " + ysql_conn_->quote(operation.time_and_value.value) + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp) + ";";
    }
  }
  return query;
}

std::string YsqlDB::DeleteBatchQuery(const std::vector<DB_Operation> &delete_ops) {
  std::string query = "";

   for (int i = 0; i < delete_ops.size(); i++) {
    const DB_Operation operation = delete_ops[i];
    if (operation.table == DataTable::Objects) {
      query += "DELETE FROM " +object_table_ + " WHERE id = " + std::to_string((operation.key)[0].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp) + ";";
    } else if (operation.table == DataTable::Edges) {
      query += "DELETE FROM " + edge_table_ + " WHERE id1 = " + std::to_string((operation.key)[0].value) + " AND id2 = " + std::to_string((operation.key)[1].value) + " AND type = " + std::to_string((operation.key)[2].value) + " AND timestamp < " + std::to_string(operation.time_and_value.timestamp) + ";";
    }
  }
  return query;
}

DB *NewYsqlDB() {
    return new YsqlDB;
}

const bool registered = DBFactory::RegisterDB("ybsql", NewYsqlDB);

} // benchmark