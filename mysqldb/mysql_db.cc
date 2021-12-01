#include "mysql_db.h"

namespace {
    const std::string DATABASE_NAME = "mysqldb.dbname";

    const std::string DATABASE_URL = "mysqldb.url";

    const std::string DATABASE_USERNAME = "mysqldb.username";

    const std::string DATABASE_PASSWORD = "mysqldb.password";

    const std::string DATABASE_PORT = "mysqldb.dbport";
    const std::string DATABASE_PORT_DEFAULT = "4000";
}

namespace benchmark {

inline PreparedStatement BuildReadObject(SuperiorMySqlpp::Connection & conn) {
  std::string object_string = "SELECT `timestamp`, `value` FROM objects WHERE id=?";
  return conn.makeDynamicPreparedStatement(object_string.c_str());
}

inline PreparedStatement BuildReadEdge(SuperiorMySqlpp::Connection & conn) {
  std::string edge_string = "SELECT `timestamp`, `value` FROM edges WHERE id1=? AND id2=? AND type=?";
  return conn.makeDynamicPreparedStatement(edge_string.c_str());
}

inline PreparedStatement BuildInsertObject(SuperiorMySqlpp::Connection & conn) {
  std::string object_string = "INSERT INTO objects (id, timestamp, value) VALUES "
      "(?, ?, ?)";
  return conn.makeDynamicPreparedStatement(object_string.c_str());
}

inline PreparedStatement BuildInsertOther(SuperiorMySqlpp::Connection & conn) {
  std::string edge_base = "INSERT INTO edges (id1, id2, type, timestamp, value) "
      "SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS ";
  std::string other_filter = "(SELECT 1 FROM edges WHERE id1=? AND type='unique' OR "
      "id1=? AND type='unique_and_bidirectional' OR id1=? AND id2=? and type='bidirectional' "
      "OR id1=? AND id2=?)";
  return conn.makeDynamicPreparedStatement((edge_base + other_filter).c_str());
}

inline PreparedStatement BuildInsertUnique(SuperiorMySqlpp::Connection & conn) {
  std::string edge_base = "INSERT INTO edges (id1, id2, type, timestamp, value) "
      "SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS ";
  std::string unique_filter = "(SELECT 1 FROM edges WHERE id1=? OR id1=? AND id2=?)";
  return conn.makeDynamicPreparedStatement((edge_base + unique_filter).c_str());
}

inline PreparedStatement BuildInsertBidirectional(SuperiorMySqlpp::Connection & conn) {
  std::string edge_base = "INSERT INTO edges (id1, id2, type, timestamp, value) "
      "SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS ";
  std::string bidirectional_filter = "(SELECT 1 FROM edges WHERE id1=? AND type='unique' OR "
      "id1=? AND type='unique_and_bidirectional' OR id1=? AND id2=? and type='other' "
      "OR id1=? AND id2=? AND type='other' OR id1=? AND id2=? AND type='unique')";
  return conn.makeDynamicPreparedStatement((edge_base + bidirectional_filter).c_str());
}

inline PreparedStatement BuildInsertUniqueAndBidirectional(SuperiorMySqlpp::Connection & conn) {
  std::string edge_base = "INSERT INTO edges (id1, id2, type, timestamp, value) "
      "SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS ";
  std::string unique_bi_filter = "(SELECT 1 FROM edges WHERE id1=? OR id1=? AND id2=? "
  "AND type='other' OR id1=? AND id2=? AND type='unique')";
  return conn.makeDynamicPreparedStatement((edge_base + unique_bi_filter).c_str());
}

inline PreparedStatement BuildDeleteObject(SuperiorMySqlpp::Connection & conn) {
  std::string object_string = "DELETE FROM objects where timestamp<? "
    "AND id=?";
  return conn.makeDynamicPreparedStatement(object_string.c_str());
}

inline PreparedStatement BuildDeleteEdge(SuperiorMySqlpp::Connection & conn) {
    std::string edge_string = "DELETE FROM edges where timestamp<? "
      "AND id1=? AND id2=? AND type=?";
    return conn.makeDynamicPreparedStatement(edge_string.c_str());
}

inline PreparedStatement BuildUpdateObject(SuperiorMySqlpp::Connection & conn) {
  std::string object_string = "UPDATE objects SET timestamp=?, value=? WHERE "
    "timestamp<? AND id=?";
  return conn.makeDynamicPreparedStatement(object_string.c_str());
}

inline PreparedStatement BuildUpdateEdge(SuperiorMySqlpp::Connection & conn) {
  std::string edge_string = "UPDATE edges SET timestamp=?, value=? WHERE "
      "timestamp<? AND id1=? AND id2=? AND type=?";
  return conn.makeDynamicPreparedStatement(edge_string.c_str());
}

MySqlDB::PreparedStatements::PreparedStatements(utils::Properties const & props) 
  : sql_connection_{props.GetProperty(DATABASE_NAME),
                    props.GetProperty(DATABASE_USERNAME),
                    props.GetProperty(DATABASE_PASSWORD),
                    props.GetProperty(DATABASE_URL),
                    std::stoi(props.GetProperty(DATABASE_PORT))}
  , read_object(BuildReadObject(sql_connection_))
  , read_edge(BuildReadEdge(sql_connection_))
  , insert_object(BuildInsertObject(sql_connection_))
  , insert_other(BuildInsertOther(sql_connection_))
  , insert_unique(BuildInsertUnique(sql_connection_))
  , insert_bidirectional(BuildInsertBidirectional(sql_connection_))
  , insert_unique_and_bidirectional(BuildInsertUniqueAndBidirectional(sql_connection_))
  , delete_object(BuildDeleteObject(sql_connection_))
  , delete_edge(BuildDeleteEdge(sql_connection_))
  , update_object(BuildUpdateObject(sql_connection_))
  , update_edge(BuildUpdateEdge(sql_connection_))
{
}

void MySqlDB::Init() {
  const utils::Properties &props = *props_;
  statements = new PreparedStatements {props};
}

void MySqlDB::Cleanup() {
  delete statements;
}

Status MySqlDB::Read(const std::string &table, const std::vector<Field> &key,
                         const std::vector<std::string> *fields, std::vector<Field> &result) {
  
  assert(table == "edges" || table == "objects");
  assert(fields && fields->size() == 2);
  assert((*fields)[0] == "timestamp");
  assert((*fields)[1] == "value");

  bool row_found = false;
  SuperiorMySqlpp::Nullable<SuperiorMySqlpp::StringDataBase<4100>> s;
  SuperiorMySqlpp::Nullable<long> timestamp;
  if (table == "edges") {
    auto & statement = statements->read_edge;
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    const char *id1 = key[0].value.c_str();
    const char *id2 = key[1].value.c_str();
    const char *type = key[2].value.c_str();
    statement.bindParam(0, id1);
    statement.bindParam(1, id2);
    statement.bindParam(2, type);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
    statement.bindResult(0, timestamp);
    statement.bindResult(1, s);
    statement.updateResultBindings();
    statement.fetch();
  } else {
    auto & statement = statements->read_object;
    assert(key.size() == 1);
    assert(key[0].name == "id");
    const char *id = key[0].value.c_str();
    statement.bindParam(0, id);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
    statement.bindResult(0, timestamp);
    statement.bindResult(1, s);
    statement.updateResultBindings();
    statement.fetch();
  }
  if (!timestamp.isValid() || !s.isValid()) {
    std::cerr << "Key not found" << std::endl;
    return Status::kNotFound;
  }
  result.emplace_back(std::move((*fields)[0]), std::to_string(timestamp.value()));
  result.emplace_back(std::move((*fields)[1]), s->getString());
  return Status::kOK;
}

Status MySqlDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields,
                         std::vector<std::vector<Field>> &result_buffer) {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(table == "edges");
  assert(key.size() == 3);
  assert(key[0].name == "id1");
  assert(key[1].name == "id2");
  assert(key[2].name == "type");
  assert(len > 0);
  assert(fields == NULL);
  std::ostringstream query_string;
  query_string << "SELECT id1, id2, type FROM edges WHERE "
               << "(id1, id2, type) > ('"
               << key[0].value << "','" << key[1].value << "','"
               << key[2].value << "') ORDER BY id1, id2, type "
               << "LIMIT " << len;
  auto query = statements->sql_connection_.makeQuery(query_string.str().c_str());
  try {
    query.execute();
  } catch (SuperiorMySqlpp::MysqlInternalError e) {
    std::cerr << e.getMysqlError() << std::endl;
    return Status::kError;
  }
  auto result = query.store();
  while (auto row = result.fetchRow()) {
    std::vector<Field> row_result;
    int i = 0;
    for (auto it = row.begin(); it != row.end(); ++it) {
      switch (i) {
        case 0:
          row_result.emplace_back("id1", (*it).getString());
          break;
        case 1:
          row_result.emplace_back("id2", (*it).getString());
          break;
        case 2:
          row_result.emplace_back("type", (*it).getString());
          break;
        default:
          throw std::invalid_argument("Error reading row in batch read.");
      }
      i++;
    }
    assert(row_result.size() == 3);
    result_buffer.push_back(std::move(row_result));
  }
  return Status::kOK;
}

Status MySqlDB::Update(const std::string &table, const std::vector<Field> &key,
                           const std::vector<Field> &values) {
  assert(table == "edges" || table == "objects");
  assert(values.size() == 2);
  assert(values[0].name == "timestamp");
  assert(values[1].name == "value");
  int64_t timestamp = std::stol(values[0].value);
  const char * val = values[1].value.c_str();

  if (table == "edges") {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    auto & statement = statements->update_edge;
    const char * id1 = key[0].value.c_str();
    const char * id2 = key[1].value.c_str();
    const char * type = key[2].value.c_str();
    statement.bindParam(0, timestamp);
    statement.bindParam(1, val);
    statement.bindParam(2, timestamp);
    statement.bindParam(3, id1);
    statement.bindParam(4, id2);
    statement.bindParam(5, type);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    auto & statement = statements->update_object;
    const char * id = key[0].value.c_str();
    statement.bindParam(0, timestamp);
    statement.bindParam(1, val);
    statement.bindParam(2, timestamp);
    statement.bindParam(3, id);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
  }
  return Status::kOK;
}

Status MySqlDB::Insert(const std::string &table, const std::vector<Field> &key,
                           const std::vector<Field> &values) {
  assert(table == "edges" || table == "objects");
  assert(values.size() == 2);
  assert(values[0].name == "timestamp");
  assert(values[1].name == "value");

  int64_t timestamp = std::stol(values[0].value);
  const char *val = values[1].value.c_str();

  if (table == "objects") {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    const char *id = key[0].value.c_str();
    auto & statement = statements->insert_object;
    statement.bindParam(0, id);
    statement.bindParam(1, timestamp);
    statement.bindParam(2, val);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    const char *id1 = key[0].value.c_str();
    const char *id2 = key[1].value.c_str();
    const char *type = key[2].value.c_str();
    if (key[2].value == "other") {
      auto & statement = statements->insert_other;
      statement.bindParam(0, id1);
      statement.bindParam(1, id2);
      statement.bindParam(2, type);
      statement.bindParam(3, timestamp);
      statement.bindParam(4, val);
      statement.bindParam(5, id1);
      statement.bindParam(6, id1);
      statement.bindParam(7, id1);
      statement.bindParam(8, id2);
      statement.bindParam(9, id2);
      statement.bindParam(10, id1);
      statement.updateParamBindings();
      try {
        statement.execute();
      } catch (SuperiorMySqlpp::MysqlInternalError e) {
        std::cerr << e.getMysqlError() << std::endl;
        return Status::kError;
      }
    } else if (key[2].value == "unique") {
      auto & statement = statements->insert_unique;
      statement.bindParam(0, id1);
      statement.bindParam(1, id2);
      statement.bindParam(2, type);
      statement.bindParam(3, timestamp);
      statement.bindParam(4, val);
      statement.bindParam(5, id1);
      statement.bindParam(6, id2);
      statement.bindParam(7, id1);
      statement.updateParamBindings();
      try {
        statement.execute();
      } catch (SuperiorMySqlpp::MysqlInternalError e) {
        std::cerr << e.getMysqlError() << std::endl;
        return Status::kError;
      }
    } else if (key[2].value == "bidirectional") {
      auto & statement = statements->insert_bidirectional;
      statement.bindParam(0, id1);
      statement.bindParam(1, id2);
      statement.bindParam(2, type);
      statement.bindParam(3, timestamp);
      statement.bindParam(4, val);
      statement.bindParam(5, id1);
      statement.bindParam(6, id1);
      statement.bindParam(7, id1);
      statement.bindParam(8, id2);
      statement.bindParam(9, id2);
      statement.bindParam(10, id1);
      statement.bindParam(11, id2);
      statement.bindParam(12, id1);
      statement.updateParamBindings();
      try {
        statement.execute();
      } catch (SuperiorMySqlpp::MysqlInternalError e) {
        std::cerr << e.getMysqlError() << std::endl;
        return Status::kError;
      }
    } else if (key[2].value == "unique_and_bidirectional") {
      auto & statement = statements->insert_unique_and_bidirectional;
      statement.bindParam(0, id1);
      statement.bindParam(1, id2);
      statement.bindParam(2, type);
      statement.bindParam(3, timestamp);
      statement.bindParam(4, val);
      statement.updateParamBindings();
      try {
        statement.execute();
      } catch (SuperiorMySqlpp::MysqlInternalError e) {
        std::cerr << e.getMysqlError() << std::endl;
        return Status::kError;
      }
    } else {
      throw std::invalid_argument("Invalid edge type!");
    }
  }
  return Status::kOK;
}

Status MySqlDB::BatchInsert(const std::string &table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values)
{
  std::lock_guard<std::mutex> lock(mutex_);
  assert(table == "edges" || table == "objects");
  return table == "edges" ? BatchInsertEdges(table, keys, values)
                          : BatchInsertObjects(table, keys, values);
}

Status MySqlDB::BatchRead(const std::string &table, int start_index, int end_index,
                          std::vector<std::vector<Field>> &result_buffer)
{
  std::lock_guard<std::mutex> lock(mutex_);
  assert(table == "edges");
  assert(end_index > start_index && start_index >= 0);
  std::ostringstream query_string;
  query_string << "SELECT id1, id2, type FROM edges ORDER BY id1, id2, type LIMIT "
               << end_index - start_index << " OFFSET " << start_index;
  auto query = statements->sql_connection_.makeQuery(query_string.str().c_str());
  try {
    query.execute();
  } catch (SuperiorMySqlpp::MysqlInternalError e) {
    std::cerr << e.getMysqlError() << std::endl;
    return Status::kError;
  }
  auto result = query.store();
  while (auto row = result.fetchRow()) {
    std::vector<Field> row_result;
    int i = 0;
    for (auto it = row.begin(); it != row.end(); ++it) {
      switch (i) {
        case 0:
          row_result.emplace_back("id1", (*it).getString());
          break;
        case 1:
          row_result.emplace_back("id2", (*it).getString());
          break;
        case 2:
          row_result.emplace_back("type", (*it).getString());
          break;
        default:
          throw std::invalid_argument("Error reading row in batch read.");
      }
      i++;
    }
    assert(row_result.size() == 3);
    result_buffer.push_back(std::move(row_result));
  }
  return Status::kOK;
}

Status MySqlDB::BatchInsertObjects(const std::string & table,
                                   const std::vector<std::vector<Field>> &keys,
                                   const std::vector<std::vector<Field>> &values)
{
  assert(keys.size() == values.size());
  assert(!keys.empty());
  std::ostringstream query_string;
  query_string << "INSERT INTO objects (id, timestamp, value) VALUES ";
  bool is_first = true;
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 1);
    assert(keys[i][0].name == "id");
    assert(values[i].size() == 2);
    assert(values[i][0].name == "timestamp");
    assert(values[i][1].name == "value");
    if (!is_first) {
      query_string << ", ";
    } else {
      is_first = false;
    }
    query_string << "('" << keys[i][0].value << 
                    "', " << values[i][0].value <<
                    ", '" << values[i][1].value << "')";
  }
  try {
    statements->sql_connection_.makeQuery(query_string.str().c_str()).execute();
  } catch (SuperiorMySqlpp::MysqlInternalError e) {
    std::cerr << "Batch insert failed: " << e.getMysqlError() << std::endl;
    return Status::kError;
  }
  return Status::kOK;
}

Status MySqlDB::BatchInsertEdges(const std::string & table,
                                 const std::vector<std::vector<Field>> &keys,
                                 const std::vector<std::vector<Field>> &values)
{
  assert(keys.size() == values.size());
  assert(!keys.empty());
  std::ostringstream query_string;
  query_string << "INSERT INTO edges (id1, id2, type, timestamp, value) VALUES ";
  bool is_first = true;
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 3);
    assert(keys[i][0].name == "id1");
    assert(keys[i][1].name == "id2");
    assert(keys[i][2].name == "type");
    assert(values[i].size() == 2);
    assert(values[i][0].name == "timestamp");
    assert(values[i][1].name == "value");
    if (!is_first) {
      query_string << ", ";
    } else {
      is_first = false;
    }
    query_string << "('" << keys[i][0].value << 
                    "', '" << keys[i][1].value << 
                    "', '" << keys[i][2].value <<
                    "', " << values[i][0].value <<
                    ", '" << values[i][1].value << "')";
  }
  try {
    statements->sql_connection_.makeQuery(query_string.str().c_str()).execute();
  } catch (SuperiorMySqlpp::MysqlInternalError) {
    std::cerr << "Batch insert failed" << std::endl;
    return Status::kError;
  }
  return Status::kOK;
}

Status MySqlDB::Delete(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values) {
  
  assert(table == "edges" || table == "objects");
  assert(values.size() >= 1);
  assert(values[0].name == "timestamp");
  int64_t timestamp = std::stol(values[0].value);
  if (table == "edges") {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    auto & statement = statements->delete_edge;
    const char * id1 = key[0].value.c_str();
    const char * id2 = key[1].value.c_str();
    const char * type = key[2].value.c_str();
    statement.bindParam(0, timestamp);
    statement.bindParam(1, id1);
    statement.bindParam(2, id2);
    statement.bindParam(3, type);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError e) {
      std::cerr << e.getMysqlError() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    auto & statement = statements->delete_object;
    const char * id = key[0].value.c_str();

    statement.bindParam(0, timestamp);
    statement.bindParam(1, id);
    statement.updateParamBindings();
    try {
      statement.execute();
    } catch (SuperiorMySqlpp::MysqlInternalError) {
      return Status::kError;
    }
  }
  return Status::kOK;
}

Status MySqlDB::Execute(const DB_Operation &op, std::vector<std::vector<Field>> &res, bool txn_op) {
  if (!txn_op) {
    std::lock_guard<std::mutex> lock(mutex_);
  }
  switch (op.operation) {
    case Operation::READ: {
      res.emplace_back();
      std::vector<std::string> fields;
      for (auto const & field : op.fields) {
        fields.push_back(field.name);
      }
      if (Read(op.table, op.key, &fields, res.back()) != Status::kOK) {
        std::cerr << "read failed" << std::endl;
        return Status::kError;
      }
      break;
    }
    case Operation::DELETE:
      if (Delete(op.table, op.key, op.fields) != Status::kOK) {
        std::cerr << "delete failed" << std::endl;
        return Status::kError;
      }
      break;
    case Operation::UPDATE:
      if (Update(op.table, op.key, op.fields) != Status::kOK) {
        std::cerr << "update failed" << std::endl;
        return Status::kError;
      }
      break;
    case Operation::INSERT:
      if (Insert(op.table, op.key, op.fields) != Status::kOK) {
        std::cerr << "insert failed" << std::endl;
        return Status::kError;
      }
      break;
    default:
      std::cerr << "invalid operation" << std::endl;
      return Status::kNotImplemented;
  }
  return Status::kOK;
}

Status MySqlDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                   std::vector<std::vector<Field>> &results,
                                   bool read_only) {
  try {
    statements->sql_connection_.makeQuery("START TRANSACTION").execute();
  } catch (SuperiorMySqlpp::MysqlInternalError) {
    std::cerr << "failed to start transaction" << std::endl;
    return Status::kError;
  }
  for (auto const & op : operations) {
    if (Execute(op, results, true) != Status::kOK) {
      std::cerr << "transaction failed" << std::endl;
      try {
      	statements->sql_connection_.makeQuery("ROLLBACK").execute();
      } catch (SuperiorMySqlpp::MysqlInternalError) {
	std::cerr << "failed to rollback" << std::endl;
      }
      return Status::kError;
    }
  }
  try {
    statements->sql_connection_.makeQuery("COMMIT").execute();
  } catch (SuperiorMySqlpp::MysqlInternalError) {
    std::cerr << "failed to commit" << std::endl;
    return Status::kError;
  }
  return Status::kOK;
}

DB *NewMySqlDB() {
    return new MySqlDB;
}

const bool registered = DBFactory::RegisterDB("mysql", NewMySqlDB);

}
