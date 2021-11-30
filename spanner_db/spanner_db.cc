#include "spanner_db.h"
#include "google/cloud/status.h"
#include <sstream>

namespace {
  const std::string READ_OBJECT = "SELECT timestamp, value FROM objects WHERE "
  "(primary_shard, spreader, id) = (@shard, @spreader, @id)";
  const std::string READ_EDGE = "SELECT timestamp, value FROM edges WHERE "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, @type)";
  const std::string INSERT_OBJECT = "INSERT INTO objects (primary_shard,spreader,id,timestamp,value) "
    "VALUES (@shard, @spreader, @id, @timestamp, @value)";
  const std::string INSERT_EDGE = "INSERT INTO edges "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type, timestamp, value) "
    "SELECT @primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, @type, @timestamp, @value "
    "FROM (SELECT 1) WHERE NOT EXISTS ";
  const std::string INSERT_EDGE_OTHER = ""
    "(SELECT 1 FROM edges WHERE "
    "(primary_shard, spreader1, id1, type) = (@primary_shard, @spreader1, @id1, 'unique') OR "
    "(primary_shard, spreader1, id1, type) = "
    "(@primary_shard, @spreader1, @id1, 'unique_and_bidirectional') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, 'bidirectional') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1))";
  const std::string INSERT_EDGE_BIDIRECTIONAL = ""
    "(SELECT 1 FROM edges WHERE "
    "(primary_shard, spreader1, id1, type) = (@primary_shard, @spreader1, @id1, 'unique') OR "
    "(primary_shard, spreader1, id1, type) = "
    "(@primary_shard, @spreader1, @id1, 'unique_and_bidirectional') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, 'other') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1, 'other') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1, 'unique'))";
  const std::string INSERT_EDGE_UNIQUE = ""
    "(SELECT 1 FROM edges WHERE "
    "(primary_shard, spreader1, id1) = (@primary_shard, @spreader1, @id1) OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1))";
  const std::string INSERT_EDGE_UNIQUE_BI = ""
    "(SELECT 1 FROM edges WHERE "
    "(primary_shard, spreader1, id1) = (@primary_shard, @spreader1, @id1) OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1, 'other') OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@remote_shard, @spreader2, @id2, @primary_shard, @spreader1, @id1, 'unique'))";
  const std::string DELETE_OBJECT = "DELETE FROM objects WHERE "
    "(primary_shard, spreader, id) = (@shard, @spreader, @id) "
    "AND timestamp < @timestamp";
  const std::string DELETE_EDGE = "DELETE FROM edges WHERE "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, @type) AND "
    "timestamp < @timestamp";
  const std::string UPDATE_OBJECT = "UPDATE objects SET "
    "timestamp = @timestamp, value = @value WHERE "
    "(primary_shard, spreader, id) = (@shard, @spreader, @id) AND timestamp < @timestamp";
  const std::string UPDATE_EDGE = "UPDATE edges SET "
    "timestamp = @timestamp, value = @value WHERE "
    "(primary_shard, spreader1, id1, remote_shard, spreader2, id2, type) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2, @type) "
    "AND timestamp < @timestamp";
  const std::string BATCH_READ = "SELECT primary_shard, id1, remote_shard, id2, type "
    "FROM edges ORDER BY id1, id2, type "
    "LIMIT @n OFFSET @start";
  const std::string SCAN = "SELECT "
    "primary_shard, spreader1, id1, remote_shard, spreader2, id2, type FROM edges WHERE "
    "((primary_shard, spreader1, id1, remote_shard, spreader2, id2) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2, @id2) AND "
    "type > @type OR "
    "(primary_shard, spreader1, id1, remote_shard, spreader2) = "
    "(@primary_shard, @spreader1, @id1, @remote_shard, @spreader2) AND id2 > @id2 OR "
    "(primary_shard, spreader1, id1) = (@primary_shard, @spreader1, @id1) AND "
    "remote_shard > @remote_shard OR "
    "primary_shard = @primary_shard AND spreader1 = @spreader1 AND id1 > @id1 OR "
    "primary_shard = @primary_shard  AND spreader1 > @spreader1 OR "
    "primary_shard > @primary_shard) AND "
    "(primary_shard < @limit_shard OR primary_shard=@limit_shard AND spreader1 < @limit_spreader) "
    "ORDER BY primary_shard, spreader1, id1, remote_shard, spreader2, id2, type "
    "LIMIT @n";

  inline int GetShardFromKey(std::string const & key) {
    size_t colon_pos = key.find(':');
    assert(colon_pos != std::string::npos);
    return std::stoi(key.substr(0, colon_pos));
  }

  inline int GetSpreaderFromKey(std::string const & key) {
    size_t first_colon_pos = key.find(':');
    assert(first_colon_pos != std::string::npos);
    size_t second_colon_pos = key.find(':', first_colon_pos + 1);
    assert(second_colon_pos !=  std::string::npos);
    assert(second_colon_pos > first_colon_pos + 1);
    return std::stoi(key.substr(first_colon_pos + 1, second_colon_pos - first_colon_pos - 1));
  }

  inline std::string GetIdFromKey(std::string const & key) {
    size_t first_colon_pos = key.find(':');
    assert(first_colon_pos != std::string::npos);
    size_t second_colon_pos = key.find(':', first_colon_pos + 1);
    assert(second_colon_pos !=  std::string::npos);
    assert (second_colon_pos > first_colon_pos + 1);
    return key.substr(second_colon_pos + 1);
  }
}

namespace benchmark {

inline spanner::Database MakeDB(utils::Properties const & props) {
  return spanner::Database(props.GetProperty("project.id"),
                           props.GetProperty("instance.id"),
                           props.GetProperty("database.id"));
}

SpannerDB::ConnectorInfo::ConnectorInfo(utils::Properties const & props)
  : client(props.spanner_conn)
{
}

void SpannerDB::Init() {
  const utils::Properties &props = *props_;
  info = new ConnectorInfo {props};
}

void SpannerDB::Cleanup() {
  delete info;
}

Status SpannerDB::Execute(const DB_Operation &op, std::vector<std::vector<Field>> &res,
     bool txn_op) {
  if (op.operation == Operation::READ) {
    auto read_only = spanner::MakeReadOnlyTransaction();
    return ExecuteWithTransaction(op, res, read_only);
  } else {
    auto commit = info->client.Commit(
      [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
        if (ExecuteWithTransaction(op, res, txn) == Status::kOK) {
          return spanner::Mutations{};
        } else {
          return google::cloud::Status(google::cloud::StatusCode::kUnknown, "op failed");
        }
      }
    );
    if (!commit) {
      std::cerr << "Write operation failed due to issues w/ single-op transaction." << std::endl;
      std::cerr << commit.status().message() << std::endl;
      return Status::kError;
    }
    return Status::kOK;
  }
}

Status SpannerDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                    std::vector<std::vector<Field>> &results,
                                    bool read_only)
{
  if (read_only) {
    auto read_only_txn = spanner::MakeReadOnlyTransaction();
    for (auto const & op : operations) {
      if (ExecuteWithTransaction(op, results, read_only_txn) != Status::kOK) {
        std::cerr << "Read transaction failed because of failure in read op." << std::endl;
        return Status::kError;
      }
    }
    return Status::kOK;
  } else {
    auto commit = info->client.Commit(
      [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
        for (auto const & op : operations ) {
          if (ExecuteWithTransaction(op, results, txn) != Status::kOK) {
            std::cerr << "Write transaction failed because of failure in write op." << std::endl;
            return google::cloud::Status(google::cloud::StatusCode::kUnknown, "op failed");
          }
        }
        return spanner::Mutations{};
      }
    );
    if (!commit) {
      std::cerr << "Write txn failed due to issues w/ commit." << std::endl;
      std::cerr << commit.status().message() << std::endl;
      return Status::kError;
    }
    return Status::kOK;
  }
}

Status SpannerDB::ExecuteWithTransaction(const DB_Operation &op,
      std::vector<std::vector<Field>> &res,
      spanner::Transaction txn) 
{
  switch (op.operation) {
    case Operation::READ: {
      res.emplace_back();
      std::vector<std::string> fields;
      for (auto const & field : op.fields) {
        fields.push_back(field.name);
      }
      if (Read(op.table, op.key, &fields, res.back(), txn) != Status::kOK) {
        std::cerr << "read failed" << std::endl;
        return Status::kError;
      }
      break;
    }
    case Operation::DELETE:
      if (Delete(op.table, op.key, op.fields, txn) != Status::kOK) {
        std::cerr << "delete failed" << std::endl;
        return Status::kError;
      }
      break;
    case Operation::UPDATE:
      if (Update(op.table, op.key, op.fields, txn) != Status::kOK) {
        std::cerr << "update failed" << std::endl;
        return Status::kError;
      }
      break;
    case Operation::INSERT:
      if (Insert(op.table, op.key, op.fields, txn) != Status::kOK) {
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


// not called externally by workload
Status SpannerDB::Read(const std::string &table, const std::vector<Field> &key,
                     const std::vector<std::string> *fields, std::vector<Field> &result) {
  return Status::kNotImplemented;
}

Status SpannerDB::Update(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values) {
  return Status::kNotImplemented;
}

Status SpannerDB::Insert(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values) {
  return Status::kNotImplemented;
}

Status SpannerDB::Delete(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values) {
  return Status::kNotImplemented;
}


Status SpannerDB::Read(const std::string &table, const std::vector<Field> &key,
                     const std::vector<std::string> *fields, std::vector<Field> &result,
                     spanner::Transaction txn) {
  
  assert(table == "edges" || table == "objects");
  assert(fields && fields->size() == 2);
  assert((*fields)[0] == "timestamp");
  assert((*fields)[1] == "value");

  std::ostringstream timestamp;
  std::string value;
  using RowType = std::tuple<int64_t, std::string>;
  int num_rows_read = 0;
  if (table == "edges") {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    spanner::SqlStatement read_edge(READ_EDGE, {
      {"primary_shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader1", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id1", spanner::Value(GetIdFromKey(key[0].value))},
      {"remote_shard", spanner::Value(GetShardFromKey(key[1].value))},
      {"spreader2", spanner::Value(GetSpreaderFromKey(key[1].value))},
      {"id2", spanner::Value(GetIdFromKey(key[1].value))},
      {"type", spanner::Value(key[2].value)}
    });
    auto rows = info->client.ExecuteQuery(txn, std::move(read_edge));
    for (auto const & row : spanner::StreamOf<RowType>(rows)) {
      if (!row) { 
        std::cerr << "Invalid row caused read to fail" << std::endl;
        std::cerr << row.status().message() << std::endl;
        return Status::kError; 
      }
      timestamp << std::get<0>(*row);
      value = std::get<1>(*row);
      ++num_rows_read;
    }
  } else {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    spanner::SqlStatement read_object(READ_OBJECT, 
      { {"shard", spanner::Value(GetShardFromKey(key[0].value))},
        {"spreader", spanner::Value(GetSpreaderFromKey(key[0].value))},
        {"id", spanner::Value(GetIdFromKey(key[0].value))}});
    auto rows = info->client.ExecuteQuery(txn, std::move(read_object));
    int num_rows = 0;
    for (auto const & row : spanner::StreamOf<RowType>(rows)) {
      if (!row) { 
        std::cerr << "Invalid row caused read to fail" << std::endl;
        std::cerr << row.status().message() << std::endl;
        return Status::kError; 
      }
      timestamp << std::get<0>(*row);
      value = std::get<1>(*row);
      ++num_rows_read;
    }
  }
  assert(num_rows_read == 0 || num_rows_read == 1);
  if (num_rows_read == 0) {
    return Status::kNotFound;
  }
  result.emplace_back("timestamp", timestamp.str());
  result.emplace_back("value", std::move(value));
  return Status::kOK;
}

Status SpannerDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
                         const std::vector<std::string> *fields,
                         std::vector<std::vector<Field>> &result_buffer,
                         const std::vector<Field> & limit) {
  assert(table == "edges");
  assert(key.size() == 3);
  assert(key[0].name == "id1");
  assert(key[1].name == "id2");
  assert(key[2].name == "type");
  assert(len > 0);
  assert(fields == NULL);
  assert(limit.size() == 3);
  assert(limit[0].name == "id1");
  assert(limit[1].name == "id2");
  assert(limit[2].name == "type");
  assert(limit[1].value == "0:0:+");
  assert(limit[2].value == "");
  auto read_only = spanner::MakeReadOnlyTransaction();
  spanner::SqlStatement scan(SCAN, {
    {"primary_shard", spanner::Value(GetShardFromKey(key[0].value))},
    {"spreader1", spanner::Value(GetSpreaderFromKey(key[0].value))},
    {"id1", spanner::Value(GetIdFromKey(key[0].value))},
    {"remote_shard", spanner::Value(GetShardFromKey(key[1].value))},
    {"spreader2", spanner::Value(GetSpreaderFromKey(key[1].value))},
    {"id2", spanner::Value(GetIdFromKey(key[1].value))},
    {"type", spanner::Value(key[2].value)},
    {"n", spanner::Value(len)},
    {"limit_shard", spanner::Value(GetShardFromKey(limit[0].value))},
    {"limit_spreader", spanner::Value(GetSpreaderFromKey(limit[0].value))}
  });
  auto rows = info->client.ExecuteQuery(read_only, std::move(scan));
  using RowType = std::tuple<int64_t, int64_t, std::string, int64_t, int64_t, std::string, std::string>;
  int rows_found = 0;
  for (auto const & row : spanner::StreamOf<RowType>(rows)) {
    if (!row) {
      std::cerr << "Invalid row caused scan to fail: " << row.status().message() << std::endl;
      return Status::kError;
    }
    std::string id1 = std::to_string(std::get<0>(*row)) + ':' + 
                      std::to_string(std::get<1>(*row)) + ':' + std::get<2>(*row);
    std::string id2 = std::to_string(std::get<3>(*row)) + ':' + 
                      std::to_string(std::get<4>(*row)) + ':' + std::get<5>(*row);
    std::string type = std::get<6>(*row);
    std::vector<Field> row_result = {{"id1", std::move(id1)}, {"id2", std::move(id2)},
        {"type", std::move(type)}};
    result_buffer.emplace_back(std::move(row_result));
    ++rows_found;
  }
  if (rows_found == 0) {
    std::cerr << "Scan did not load any rows" << std::endl;
  }
  return Status::kOK;
}

Status SpannerDB::Update(const std::string &table, const std::vector<Field> &key,
                      const std::vector<Field> &values, spanner::Transaction txn) {
  assert(table == "edges" || table == "objects");
  assert(values.size() == 2);
  assert(values[0].name == "timestamp");
  assert(values[1].name == "value");

  if (table == "edges") {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    spanner::SqlStatement update_edge(UPDATE_EDGE, {
      {"primary_shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader1", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id1", spanner::Value(GetIdFromKey(key[0].value))},
      {"remote_shard", spanner::Value(GetShardFromKey(key[1].value))},
      {"spreader2", spanner::Value(GetSpreaderFromKey(key[1].value))},
      {"id2", spanner::Value(GetIdFromKey(key[1].value))},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(std::stol(values[0].value))},
      {"value", spanner::Value(values[1].value)}
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(update_edge));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    spanner::SqlStatement update_object(UPDATE_OBJECT, {
      {"shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id", spanner::Value(GetIdFromKey(key[0].value))},
      {"timestamp", spanner::Value(std::stol(values[0].value))},
      {"value", spanner::Value(values[1].value)}
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(update_object));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  }
  return Status::kOK;
}

Status SpannerDB::Insert(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values, spanner::Transaction txn) {
  assert(table == "edges" || table == "objects");
  assert(values.size() == 2);
  assert(values[0].name == "timestamp");
  assert(values[1].name == "value");

  if (table == "objects") {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    spanner::SqlStatement insert_object(INSERT_OBJECT, {
      {"shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id", spanner::Value(GetIdFromKey(key[0].value))},
      {"timestamp", spanner::Value(std::stol(values[0].value))},
      {"value", spanner::Value(values[1].value)}
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(insert_object));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    std::string type = key[2].value;
    std::string read_string;
    if (type == "other") {
      read_string = INSERT_EDGE_OTHER;
    } else if (type == "unique") {
      read_string = INSERT_EDGE_UNIQUE;
    } else if (type == "bidirectional") {
      read_string = INSERT_EDGE_BIDIRECTIONAL;
    } else if (type == "unique_and_bidirectional") {
      read_string = INSERT_EDGE_UNIQUE_BI;
    } else {
      throw std::runtime_error("Invalid edge type!");
    }
    spanner::SqlStatement insert_edge(INSERT_EDGE + read_string, {
      {"primary_shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader1", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id1", spanner::Value(GetIdFromKey(key[0].value))},
      {"remote_shard", spanner::Value(GetShardFromKey(key[1].value))},
      {"spreader2", spanner::Value(GetSpreaderFromKey(key[1].value))},
      {"id2", spanner::Value(GetIdFromKey(key[1].value))},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(std::stol(values[0].value))},
      {"value", spanner::Value(values[1].value)}
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(insert_edge));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  }
  return Status::kOK;
}

Status SpannerDB::BatchInsert(const std::string &table,
                            const std::vector<std::vector<Field>> &keys,
                            const std::vector<std::vector<Field>> &values)
{
  assert(table == "edges" || table == "objects");
  return table == "edges" ? BatchInsertEdges(table, keys, values)
                          : BatchInsertObjects(table, keys, values);
}

Status SpannerDB::BatchRead(const std::string &table, int start_index, int end_index,
                          std::vector<std::vector<Field>> &result_buffer)
{
  std::cerr << "Batch reads are not implemented!" << std::endl;
  return Status::kNotImplemented;
}

Status SpannerDB::BatchInsertObjects(const std::string & table,
                                   const std::vector<std::vector<Field>> &keys,
                                   const std::vector<std::vector<Field>> &values)
{
  auto insert_objects_builder = spanner::InsertMutationBuilder("objects",
      {"primary_shard", "spreader", "id", "timestamp", "value"});
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 1);
    assert(keys[i][0].name == "id");
    assert(values[i].size() == 2);
    assert(values[i][0].name == "timestamp");
    assert(values[i][1].name == "value");
    insert_objects_builder = insert_objects_builder.EmplaceRow(
        GetShardFromKey(keys[i][0].value),
        GetSpreaderFromKey(keys[i][0].value),
        GetIdFromKey(keys[i][0].value),
        std::stol(values[i][0].value),
        values[i][1].value
    );
  }
  auto insert_objects = insert_objects_builder.Build();
  auto commit_result = info->client.Commit(spanner::Mutations{insert_objects});
  if (!commit_result) {
    std::cerr << "Batch insert (objects) failed: " << commit_result.status().message() << std::endl;
    return Status::kError;
  }
  return Status::kOK;
}

Status SpannerDB::BatchInsertEdges(const std::string & table,
                                 const std::vector<std::vector<Field>> &keys,
                                 const std::vector<std::vector<Field>> &values)
{
  auto insert_edges_builder = spanner::InsertMutationBuilder("edges", 
      {"primary_shard", "spreader1", "id1", "remote_shard", "spreader2", 
       "id2", "type", "timestamp", "value"});
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 3);
    assert(keys[i][0].name == "id1");
    assert(keys[i][1].name == "id2");
    assert(keys[i][2].name == "type");
    assert(values[i].size() == 2);
    assert(values[i][0].name == "timestamp");
    assert(values[i][1].name == "value");
    insert_edges_builder = insert_edges_builder.EmplaceRow(
        GetShardFromKey(keys[i][0].value),
        GetSpreaderFromKey(keys[i][0].value),
        GetIdFromKey(keys[i][0].value),
        GetShardFromKey(keys[i][1].value),
        GetSpreaderFromKey(keys[i][1].value),
        GetIdFromKey(keys[i][1].value),
        keys[i][2].value,
        std::stol(values[i][0].value),
        values[i][1].value
    );
  }
  auto insert_edges = insert_edges_builder.Build();
  auto commit_result = info->client.Commit(spanner::Mutations{insert_edges});
  if (!commit_result) {
    std::cerr << "Batch insert (edges) failed: " << commit_result.status().message() << std::endl;
    return Status::kError;
  }
  return Status::kOK;
}

Status SpannerDB::Delete(const std::string &table, const std::vector<Field> &key,
                       const std::vector<Field> &values,
                       spanner::Transaction txn) {
  assert(table == "edges" || table == "objects");
  assert(values.size() == 2);
  assert(values[0].name == "timestamp");
  assert(values[1].name == "value");

  if (table == "edges") {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    spanner::SqlStatement delete_edge(DELETE_EDGE, {
      {"primary_shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader1", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id1", spanner::Value(GetIdFromKey(key[0].value))},
      {"remote_shard", spanner::Value(GetShardFromKey(key[1].value))},
      {"spreader2", spanner::Value(GetSpreaderFromKey(key[1].value))},
      {"id2", spanner::Value(GetIdFromKey(key[1].value))},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(std::stol(values[0].value))}
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(delete_edge));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  } else {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    spanner::SqlStatement delete_object(DELETE_OBJECT, {
      {"shard", spanner::Value(GetShardFromKey(key[0].value))},
      {"spreader", spanner::Value(GetSpreaderFromKey(key[0].value))},
      {"id", spanner::Value(GetIdFromKey(key[0].value))},
      {"timestamp", spanner::Value(std::stol(values[0].value))},
    });
    auto dml_result = info->client.ExecuteDml(txn, std::move(delete_object));
    if (!dml_result) {
      std::cerr << dml_result.status().message() << std::endl;
      return Status::kError;
    }
  }
  return Status::kOK;
}

DB *NewSpannerDB() {
    return new SpannerDB;
}

const bool registered = DBFactory::RegisterDB("spanner", NewSpannerDB);

}
