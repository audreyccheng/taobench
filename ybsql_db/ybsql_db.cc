#include "ybsql_db.h"
#include "db_factory.h"
#include <pqxx/pqxx>
#include "pqxx/nontransaction"


namespace {
    const std::string DATABASE_STRING = "ybsql_db.string";
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
    edge_table_ = props_->GetProperty("edge_table_", "edges");
    object_table_ = props_->GetProperty("object_table_", "objects");
    ysql_conn_->prepare("read_object", "SELECT timestamp, value FROM " +object_table_ + " WHERE id = $1");
    ysql_conn_->prepare("read_edge", "SELECT timestamp, value FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3");
    ysql_conn_->prepare("scan_object", "SELECT id FROM " +object_table_ + " WHERE yb_hash_code(id) >= $1 AND yb_hash_code(id) < $2 LIMIT $3");
    ysql_conn_->prepare("scan_edge", "SELECT id1, id2, type FROM " + edge_table_ + " WHERE yb_hash_code(id1) >= $1 AND yb_hash_code(id1) < $2 LIMIT $3");
    ysql_conn_->prepare("update_object", "UPDATE " +object_table_ + " SET timestamp = $1, value = $2 WHERE id = $3 AND timestamp < $1");
    ysql_conn_->prepare("update_edge", "UPDATE " + edge_table_ + " SET timestamp = $1, value = $2 WHERE id1 = $3 AND id2 = $4 AND type = $5 AND timestamp < $1");
    ysql_conn_->prepare("insert_object", "INSERT INTO " +object_table_ + " (id, timestamp, value) SELECT $1, $2, $3");
    ysql_conn_->prepare("insert_edge_other", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1::VARCHAR, $2::VARCHAR, 'other', $3, $4 WHERE NOT EXISTS (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 AND type='unique' OR id1=$1 AND type='unique_and_bidirectional' OR id1=$1 AND id2=$2 and type='bidirectional' OR id1=$1 AND id2=$1)");
    ysql_conn_->prepare("insert_edge_bidirectional", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1::VARCHAR, $2::VARCHAR, 'bidirectional', $3, $4 WHERE NOT EXISTS (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 AND type='unique' OR id1=$1 AND type='unique_and_bidirectional' OR id1=$1 AND id2=$2 and type='other' OR id1=$1 AND id2=$2 AND type='other' OR id1=$1 AND id2=$2 AND type='unique')");
    ysql_conn_->prepare("insert_edge_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1::VARCHAR, $2::VARCHAR, 'unique', $3, $4 WHERE NOT EXISTS (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 OR id1=$1 AND id2=$2)");
    ysql_conn_->prepare("insert_edge_bi_unique", "INSERT INTO " + edge_table_ + " (id1, id2, type, timestamp, value) SELECT $1::VARCHAR, $2::VARCHAR, 'unique_and_bidirectional', $3, $4 WHERE NOT EXISTS (SELECT 1 FROM " + edge_table_ + " WHERE id1=$1 OR id1=$1 AND id2=$2 AND type='other' OR id1=$1 AND id2=$2 AND type='unique')");
    ysql_conn_->prepare("delete_object", "DELETE FROM " +object_table_ + " WHERE id = $1 AND timestamp < $2");
    ysql_conn_->prepare("delete_edge", "DELETE FROM " + edge_table_ + " WHERE id1 = $1 AND id2 = $2 AND type = $3 AND timestamp < $4");
    // not used
    ysql_conn_->prepare("batch_read", "SELECT id1, id2, type FROM " + edge_table_ + " LIMIT $1 OFFSET $2");


}

void YsqlDB::Cleanup() {
  ysql_conn_->close();
  delete ysql_conn_;
}

Status YsqlDB::Read(const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields, std::vector<Field> &result) {
    assert(table == "edges" || table == "objects");
    assert(fields && fields->size() == 2);
    assert((*fields)[0] == "timestamp");
    assert((*fields)[1] == "value");

    const std::lock_guard<std::mutex> lock(mu_);
    // Execute SQL commands
    try {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoRead(tx, table, key, fields);

      for (size_t i = 0; i < fields->size(); i++) {
        Field f((*fields)[i], (r[0][i]).as<std::string>("NULL"));
        result.emplace_back(f);
      }
      return Status::kOK;
    }
    catch (const std::exception &e) {
      //std::cerr << e.what() << std::endl;
      return Status::kError;
    }
}

/* Helper function to execute the read prepare statement 
   Objects: key[0] = id
   Edges: key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoRead(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields) {
  if (table == "objects") {
    return tx.exec_prepared("read_object", key[0].value);
  } else if (table == "edges") {
    return tx.exec_prepared("read_edge", key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}


Status YsqlDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields,
                         std::vector<std::vector<Field>> &result, int start, int end) {
    const std::lock_guard<std::mutex> lock(mu_);
    assert(table == "edges");
    assert(fields == NULL);
    std::vector<std::string> hardcoded_fields{"id1", "id2", "type"};
    if (fields == NULL) {
      fields = &hardcoded_fields;
    }

    try {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoScan(tx, table, key, len, fields, start, end);
      for (auto row : r) {
        std::vector<Field> rowVector;
        for (int j = 0; j < fields->size(); j++) {
          Field f((*fields)[j], (row)[j].as<std::string>("NULL"));
          rowVector.push_back(f);
        }
        result.push_back(rowVector);
      }
      return Status::kOK;
    }
    catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
      return Status::kError;
    }
}

/* Helper function to execute the scan prepare statement 
   start: the start of the yb_hash_code(id) to scan
   end: the end of the yb_hash_code(id) to scan */
pqxx::result YsqlDB::DoScan(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields, int start, int end) {
  if (table == "objects") {
    return tx.exec_prepared("scan_object", start, end, len);
  } else if (table == "edges") {
    return tx.exec_prepared("scan_edge", start, end, len);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status YsqlDB::Update(const std::string &table, const std::vector<Field> &key,
                          const std::vector<Field> &values) {
    
    assert(table == "edges" || table == "objects");
    assert(values.size() == 2);
    assert(values[0].name == "timestamp");
    assert(values[1].name == "value");
    const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoUpdate(tx, table, key, values);     
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      //std::cerr << e.what() << std::endl;
      return Status::kError;
    }
}

/* Helper function to execute the update prepare statement 
   Objects: values[0] = timestamp, values[1] = value, key[0] = id
   Edges: values[0] = timestamp, values[1] = value, key[0] = id1, key[1] = id2, key[2] = type */
pqxx::result YsqlDB::DoUpdate(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == "objects") {
    return tx.exec_prepared("update_object", values[0].value, values[1].value, key[0].value);
  } else if (table == edge_table_) {
    return tx.exec_prepared("update_edge", values[0].value, values[1].value, key[0].value, key[1].value, key[2].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}


Status YsqlDB::Insert(const std::string &table, const std::vector<Field> &key,
                          const std::vector<Field> &values) {
    assert(table == "edges" || table == "objects");
    assert(values.size() == 2);
    assert(values[0].name == "timestamp");
    assert(values[1].name == "value");
    assert(!key.empty());
    const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoInsert(tx, table, key, values);      
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      //std::cerr << e.what() << std::endl;
      return Status::kError;
    }
}

/* Helper function to execute the insert prepare statement 
   Objects: key[0] = id, values[0] = timestamp, values[1] = value
   Edges: key[0] = id1, key[1] = id2, key[2] = type, values[0] = timestamp, values[1] = value */
pqxx::result YsqlDB::DoInsert(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == "objects") {
    return tx.exec_prepared("insert_object", key[0].value, values[0].value, values[1].value);
  } else if (table == "edges") {
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

Status YsqlDB::BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                             const std::vector<std::vector<Field>> &values) {
    const std::lock_guard<std::mutex> lock(mu_);
    assert(table == "edges" || table == "objects");
    return table == "edges" ? BatchInsertEdges(table, keys, values)
                            : BatchInsertObjects(table, keys, values);
}

/* Helper function to do batch insert for objects */
Status YsqlDB::BatchInsertObjects(const std::string & table,
                                   const std::vector<std::vector<Field>> &keys,
                                   const std::vector<std::vector<Field>> &values) {
  assert(keys.size() == values.size());
  assert(!keys.empty());
  try {
    pqxx::nontransaction tx(*ysql_conn_);
    std::string query;
    query += "INSERT INTO " + object_table_ + " (id, timestamp, value) VALUES ";
    bool is_first = true;
    for (size_t i = 0; i < keys.size(); ++i) {
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
      query += "(" + ysql_conn_->quote(keys[i][0].value) +                   // id
                      ", " + values[i][0].value +                            // timestamp
                      ", " + ysql_conn_->quote(values[i][1].value) + ")";    // value
    }
    pqxx::result r = tx.exec(query);      
    return Status::kOK;
  } catch (const std::exception &e) {
    std::cerr << "Batch insert object failed:" << e.what() << std::endl;
    return Status::kError;
  }
}

/* Helper function to do batch insert for edges */
Status YsqlDB::BatchInsertEdges(const std::string & table,
                                 const std::vector<std::vector<Field>> &keys,
                                 const std::vector<std::vector<Field>> &values)
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
      assert(values[i].size() == 2);
      assert(values[i][0].name == "timestamp");
      assert(values[i][1].name == "value");

      if (!is_first) {
        query += ", ";
      } else {
        is_first = false;
      }
      query += "(" + ysql_conn_->quote(keys[i][0].value)         // id1
                + ", " + ysql_conn_->quote(keys[i][1].value)     // id2
                + ", " + ysql_conn_->quote(keys[i][2].value)     // type
                + ", " + values[i][0].value                      // timestamp
                + ", " + ysql_conn_->quote(values[i][1].value)   // value
              + ")";
    }

    pqxx::result queryRes = tx.exec(query);

    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}


/* Since we are using yb_hash_code during batch read phase, this function is not called. 
   Instead, we use the scan function from above which pass in a start and end parameters */
Status YsqlDB::BatchRead(const std::string &table, int start_index, int end_index,
                         std::vector<std::vector<Field>> &result) {
  const std::lock_guard<std::mutex> lock(mu_);
  assert(table == "edges");
  assert(end_index > start_index && start_index >= 0);
  try {
    pqxx::nontransaction tx(*ysql_conn_);
    std::cout << "SELECT id1, id2, type FROM edges LIMIT " << end_index - start_index << " OFFSET " << start_index << std::endl;
    pqxx::result queryRes = tx.exec_prepared("batch_read", end_index - start_index, start_index);

    std::vector<std::string> fields;
    fields.push_back("id1");
    fields.push_back("id2");
    fields.push_back("type");
    for (auto row : queryRes) {
      std::vector<Field> oneRowVector;
      for (int j = 0; j < fields.size(); j++) {
        Field f(fields[j], (row)[j].as<std::string>("NULL"));
        oneRowVector.push_back(f);
      }
      result.push_back(oneRowVector);
    }
    return Status::kOK;
  } catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}   

Status YsqlDB::Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {

    const std::lock_guard<std::mutex> lock(mu_);
    try
    {
      pqxx::nontransaction tx(*ysql_conn_);
      pqxx::result r = DoDelete(tx, table, key, values);     
      return Status::kOK;
    }
    catch (const std::exception &e)
    {
      return Status::kError;
    }
}


/* Helper function to execute the delete prepare statement 
   Objects: key[0] = id, values[0] = timestamp
   Edges: key[0] = id1, key[1] = id2, key[2] = type, values[0] = timestamp */
pqxx::result YsqlDB::DoDelete(pqxx::transaction_base &tx, const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {
  if (table == "objects") {
    return tx.exec_prepared("delete_object", key[0].value, values[0].value);
  } else if (table == "edges") {
    return tx.exec_prepared("delete_edge", key[0].value, key[1].value, key[2].value, values[0].value);
  } else {
    throw std::invalid_argument("Received unknown table");
  }
}

Status YsqlDB::Execute(const DB_Operation &operation,
                           std::vector<std::vector<Field>> &result, bool tx_op) {
  try {
    switch (operation.operation) {
    case Operation::READ: {
      result.emplace_back();
      std::vector<std::string> read_fields;
      for (auto &field : operation.fields) {
        read_fields.push_back(field.name);
      }
      return Read(operation.table, operation.key, &read_fields, result.back());
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
      return Scan(operation.table, operation.key, operation.len, &scan_fields, result, 0, 1);
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
    //std::cerr << e.what() << std::endl;
    return Status::kError;
  }
}

Status YsqlDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                      std::vector<std::vector<Field>> &results, bool read_only) {                     
  try {
    const std::lock_guard<std::mutex> lock(mu_);
    pqxx::work tx(*ysql_conn_);
    
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
      // Not reached. We do not have scan inside transaction.
      case Operation::SCAN: {
        std::vector<std::string> scan_fields;
        for (auto &field : operation.fields) {
          scan_fields.push_back(field.name);
        }
        queryRes = DoScan(tx, operation.table, operation.key, operation.len, &scan_fields, 0, 4096);
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
            Field f(operation.fields[j].name, (row)[j].as<std::string>());
            oneRowVector.push_back(f);
          }
          results.push_back(oneRowVector);
        }
      }
    }
    tx.commit();
    return Status::kOK;
  } catch (std::exception const &e) {
    return Status::kError;
  }
}

DB *NewYsqlDB() {
    return new YsqlDB;
}

const bool registered = DBFactory::RegisterDB("ybsql", NewYsqlDB);

} // benchmark
