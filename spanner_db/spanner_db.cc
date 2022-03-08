#include "spanner_db.h"
#include "google/cloud/status.h"
#include <sstream>

namespace {

  static_assert(static_cast<int64_t>(benchmark::EdgeType::Unique) == 0L);
  static_assert(static_cast<int64_t>(benchmark::EdgeType::Bidirectional) == 1L);
  static_assert(static_cast<int64_t>(benchmark::EdgeType::UniqueAndBidirectional) == 2L);
  static_assert(static_cast<int64_t>(benchmark::EdgeType::Other) == 3L);

  const std::string READ_OBJECT = "SELECT timestamp, value FROM objects WHERE id = @id";
  const std::string READ_EDGE = "SELECT timestamp, value FROM edges WHERE "
    "(id1, id2, type) = (@id1, @id2, @type)";
  const std::string INSERT_OBJECT = "INSERT INTO objects (id, timestamp, value) "
    "VALUES (@id, @timestamp, @value)";
  const std::string INSERT_EDGE = "INSERT INTO edges "
    "(id1, id2, type, timestamp, value) "
    "SELECT @id1, @id2, @type, @timestamp, @value "
    "FROM (SELECT 1) WHERE NOT EXISTS ";
  const std::string INSERT_EDGE_OTHER = ""
    "(SELECT 1 FROM edges WHERE "
    "(id1, type) = (@id1, 0) OR "
    "(id1, type) = (@id1, 2) OR "
    "(id1, id2, type) = (@id1, @id2, 1) OR "
    "(id1, id2) = (@id2, @id1))";
  const std::string INSERT_EDGE_BIDIRECTIONAL = ""
    "(SELECT 1 FROM edges WHERE "
    "(id1, type) = (@id1, 0) OR "
    "(id1, type) = (@id1, 2) OR "
    "(id1, id2, type) = (@id1, @id2, 3) OR "
    "(id1, id2, type) = (@id2, @id1, 3) OR "
    "(id1, id2, type) = (@id2, @id1, 0))";
  const std::string INSERT_EDGE_UNIQUE = ""
    "(SELECT 1 FROM edges WHERE "
    "id1 = @id1 OR (id1, id2) = (@id2, @id1))";
  const std::string INSERT_EDGE_UNIQUE_BI = ""
    "(SELECT 1 FROM edges WHERE "
    "id1 = @id1 OR "
    "(id1, id2, type) = (@id2, @id1, 3) OR "
    "(id1, id2, type) = (@id2, @id1, 0))";
  const std::string DELETE_OBJECT = "DELETE FROM objects WHERE "
    "id = @id AND timestamp < @timestamp";
  const std::string DELETE_EDGE = "DELETE FROM edges WHERE "
    "(id1, id2, type) = (@id1, @id2, @type) AND "
    "timestamp < @timestamp";
  const std::string UPDATE_OBJECT = "UPDATE objects SET "
    "timestamp = @timestamp, value = @value WHERE "
    "id = @id AND timestamp < @timestamp";
  const std::string UPDATE_EDGE = "UPDATE edges SET "
    "timestamp = @timestamp, value = @value WHERE "
    "(id1, id2, type) = (@id1, @id2, @type) "
    "AND timestamp < @timestamp";
  const std::string BATCH_READ = "SELECT "
    "id1, id2, type FROM edges WHERE "
    "((id1, id2) = (@fid1, @fid2) AND type > @ftype OR "
    "id1 = @fid1 AND id2 > @fid2 OR "
    "id1 > @fid1) AND "
    "(id1 < @cid1 OR "
    "id1 = @cid1 AND id2 < @cid2 OR "
    "(id1, id2) = (@cid1, @cid2) AND type < @ctype) "
    "ORDER BY id1, id2, type "
    "LIMIT @n";

  namespace spanner = ::google::cloud::spanner;

  inline spanner::KeySet BuildEdgeKeySet(std::vector<std::vector<benchmark::DB::Field>> const & keys) {
    auto keyset = spanner::KeySet();
    for (auto const & key : keys) {
      assert(key.size() == 3);
      assert(key[0].name == "id1");
      assert(key[1].name == "id2");
      assert(key[2].name == "type");
      keyset = keyset.AddKey(
        spanner::MakeKey(key[0].value,
                         key[1].value,
                         key[2].value)
      );
    }
    return keyset;
  }

  inline spanner::KeySet BuildObjectKeySet(std::vector<std::vector<benchmark::DB::Field>> const & keys) {
    auto keyset = spanner::KeySet();
    for (auto const & key : keys) {
      assert(key.size() == 1);
      assert(key[0].name == "id");
      keyset = keyset.AddKey(spanner::MakeKey(key[0].value));
    }
    return keyset;
  }

  inline spanner::SqlStatement GetUpdateEdgeSql(std::vector<benchmark::DB::Field> const & key, 
                                                benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    return spanner::SqlStatement(UPDATE_EDGE, {
      {"id1", spanner::Value(key[0].value)},
      {"id2", spanner::Value(key[1].value)},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(timeval.timestamp)},
      {"value", spanner::Value(timeval.value)}
    });
  }

  inline spanner::SqlStatement GetUpdateObjectSql(std::vector<benchmark::DB::Field> const & key,
                                                  benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    return spanner::SqlStatement(UPDATE_OBJECT, {
      {"id", spanner::Value(key[0].value)},
      {"timestamp", spanner::Value(timeval.timestamp)},
      {"value", spanner::Value(timeval.value)}
    });
  }

  inline spanner::SqlStatement GetInsertEdgeSql(std::vector<benchmark::DB::Field> const & key,
                                                benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    benchmark::EdgeType type = static_cast<benchmark::EdgeType>(key[2].value);
    std::string read_string;
    if (type == benchmark::EdgeType::Other) {
      read_string = INSERT_EDGE_OTHER;
    } else if (type == benchmark::EdgeType::Unique) {
      read_string = INSERT_EDGE_UNIQUE;
    } else if (type == benchmark::EdgeType::Bidirectional) {
      read_string = INSERT_EDGE_BIDIRECTIONAL;
    } else if (type == benchmark::EdgeType::UniqueAndBidirectional) {
      read_string = INSERT_EDGE_UNIQUE_BI;
    } else {
      throw std::runtime_error("Invalid edge type!");
    }
    return spanner::SqlStatement(INSERT_EDGE + read_string, {
      {"id1", spanner::Value(key[0].value)},
      {"id2", spanner::Value(key[1].value)},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(timeval.timestamp)},
      {"value", spanner::Value(timeval.value)}
    });
  }

  inline spanner::SqlStatement GetInsertObjectSql(std::vector<benchmark::DB::Field> const & key,
                                                  benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    return spanner::SqlStatement(INSERT_OBJECT, {
      {"id", spanner::Value(key[0].value)},
      {"timestamp", spanner::Value(timeval.timestamp)},
      {"value", spanner::Value(timeval.value)}
    });
  }

  inline spanner::SqlStatement GetDeleteEdgeSql(std::vector<benchmark::DB::Field> const & key,
                                                benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 3);
    assert(key[0].name == "id1");
    assert(key[1].name == "id2");
    assert(key[2].name == "type");
    return spanner::SqlStatement(DELETE_EDGE, {
      {"id1", spanner::Value(key[0].value)},
      {"id2", spanner::Value(key[1].value)},
      {"type", spanner::Value(key[2].value)},
      {"timestamp", spanner::Value(timeval.timestamp)}
    });

  }

  inline spanner::SqlStatement GetDeleteObjectSql(std::vector<benchmark::DB::Field> const & key,
                                                  benchmark::DB::TimestampValue const & timeval)
  {
    assert(key.size() == 1);
    assert(key[0].name == "id");
    return spanner::SqlStatement(DELETE_OBJECT, {
      {"id", spanner::Value(key[0].value)},
      {"timestamp", spanner::Value(timeval.timestamp)},
    });
  }

  inline spanner::SqlStatement GetBatchReadSql(
      std::vector<benchmark::DB::Field> const & floor_key,
      std::vector<benchmark::DB::Field> const & ceil_key,
      int n)
  {
    assert(floor_key.size() == 3);
    assert(floor_key[0].name == "id1");
    assert(floor_key[1].name == "id2");
    assert(floor_key[2].name == "type");
    assert(ceil_key.size() == 3);
    assert(ceil_key[0].name == "id1");
    assert(ceil_key[1].name == "id2");
    assert(ceil_key[2].name == "type");
    return spanner::SqlStatement(BATCH_READ, {
      {"fid1", spanner::Value(floor_key[0].value)},
      {"fid2", spanner::Value(floor_key[1].value)},
      {"ftype", spanner::Value(floor_key[2].value)},
      {"cid1", spanner::Value(ceil_key[0].value)},
      {"cid2", spanner::Value(ceil_key[1].value)},
      {"ctype", spanner::Value(ceil_key[2].value)},
      {"n", spanner::Value(n)}
    });
  }

  inline spanner::Client MakeClient(benchmark::utils::Properties const & props) {
    auto db = spanner::Database(props.GetProperty("project.id"),
                                props.GetProperty("instance.id"),
                                props.GetProperty("database.id"));
    auto connection = spanner::MakeConnection(db);
    return spanner::Client(connection);
  }

  inline bool IsContentionMessage(std::string const & msg) {
    return msg.find("Transaction was aborted") != std::string::npos;
  }
}

namespace benchmark {

SpannerDB::ConnectorInfo::ConnectorInfo(utils::Properties const & props)
  : client(MakeClient(props))
{
}

void SpannerDB::Init() {
  info = new ConnectorInfo {*props_};
}

void SpannerDB::Cleanup() {
  delete info;
}

Status SpannerDB::Execute(const DB_Operation &op, std::vector<TimestampValue> &read_buffer, bool txn_op) {
  switch (op.operation) {
    case Operation::READ:
      return Read(op.table, op.key, read_buffer);
    case Operation::DELETE:
      return Delete(op.table, op.key, op.time_and_value);
    case Operation::UPDATE:
      return Update(op.table, op.key, op.time_and_value);
    case Operation::INSERT:
      return Insert(op.table, op.key, op.time_and_value);
    default:
      std::cerr << "invalid operation" << std::endl;
      return Status::kNotImplemented;
  }
}

Status SpannerDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                     std::vector<TimestampValue> &read_buffer,
                                     bool read_only)
{
  assert(!operations.empty());
  std::vector<std::vector<Field>> edge_keys;
  std::vector<std::vector<Field>> object_keys;
  std::unordered_set<int64_t> edge_ids;
  std::unordered_set<int64_t> object_ids;
  if (read_only) {
    using RowType = std::tuple<int64_t, std::string>;
    for (auto const & op : operations) {
      assert(op.operation == Operation::READ);
      if (op.table == DataTable::Edges && edge_ids.find(op.key[0].value) == edge_ids.end()) {
        edge_keys.emplace_back(op.key);
        edge_ids.insert(op.key[0].value);
      } else if (object_ids.find(op.key[0].value) == object_ids.end()) {
        object_keys.emplace_back(op.key);
        object_ids.insert(op.key[0].value);
      }
    }
    auto read_only_txn = spanner::MakeReadOnlyTransaction();
    int num_rows_read = 0;
    if (!edge_keys.empty()) {
      auto edge_rows = info->client.Read(read_only_txn, DataTableToStr(DataTable::Edges),
          BuildEdgeKeySet(edge_keys), {"timestamp", "value"});
      for (auto const & row : spanner::StreamOf<RowType>(edge_rows)) {
        if (!row) { 
          std::cerr << "Read Transaction failed: " << row.status().message() << std::endl;
          return Status::kError; 
        }
        read_buffer.emplace_back(std::get<0>(*row), std::get<1>(*row));
        ++num_rows_read;
      }
    }
    if (!object_keys.empty()) {
      auto object_rows = info->client.Read(read_only_txn, DataTableToStr(DataTable::Objects),
          BuildObjectKeySet(object_keys), {"timestamp", "value"});
      for (auto const & row : spanner::StreamOf<RowType>(object_rows)) {
        if (!row) { 
          std::cerr << "Read Transaction failed: " << row.status().message() << std::endl;
          return Status::kError; 
        }
        read_buffer.emplace_back(std::get<0>(*row), std::get<1>(*row));
        ++num_rows_read;
      }
    }
    if (num_rows_read < object_ids.size() + edge_ids.size()) {
      std::cerr << "Warning: " << object_ids.size() + edge_ids.size() << " unique read requests sent in read "
         "transaction but only " << num_rows_read << " rows read." << std::endl;
    }
    return Status::kOK;
  } else {
    std::vector<spanner::SqlStatement> dml_statements;
    for (auto const & op : operations) {
      switch (op.operation) {
        case Operation::UPDATE:
          dml_statements.emplace_back(op.table == DataTable::Edges
              ? GetUpdateEdgeSql(op.key, op.time_and_value) 
              : GetUpdateObjectSql(op.key, op.time_and_value));
          break; 
        case Operation::INSERT:
          dml_statements.emplace_back(op.table == DataTable::Edges 
              ? GetInsertEdgeSql(op.key, op.time_and_value) 
              : GetInsertObjectSql(op.key, op.time_and_value));
          break;
        case Operation::DELETE:
          dml_statements.emplace_back(op.table == DataTable::Edges 
              ? GetDeleteEdgeSql(op.key, op.time_and_value) 
              : GetDeleteObjectSql(op.key, op.time_and_value));
          break;
        default:
          throw std::invalid_argument("Invalid operation type in write transaction.");
      }
    }
    auto commit = info->client.Commit(
      [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
        auto result = info->client.ExecuteBatchDml(txn, dml_statements);
        if (!result) { return result.status(); }
        if (!result->status.ok()) { return result->status; }
        return spanner::Mutations{};
      }
    );
    if (!commit) {
      std::cerr << "Write transaction failed: " << commit.status().message() << std::endl;
      return IsContentionMessage(commit.status().message()) ? Status::kContentionError : Status::kError;
    }
    return Status::kOK;
  }
}

Status SpannerDB::Read(DataTable table, const std::vector<Field> &key, std::vector<TimestampValue> & buffer)
{

  using RowType = std::tuple<int64_t, std::string>;
  int num_rows_read = 0;
  spanner::KeySet keyset = table == DataTable::Edges ? BuildEdgeKeySet({key}) : BuildObjectKeySet({key});
  auto rows = info->client.Read(DataTableToStr(table), keyset, {"timestamp", "value"});
  for (auto const & row : spanner::StreamOf<RowType>(rows)) {
    if (!row) { 
      std::cerr << "Read Failed: " << row.status().message() << std::endl;
      return Status::kError; 
    }
    buffer.emplace_back(std::get<0>(*row), std::get<1>(*row));
    ++num_rows_read;
  }
  assert(num_rows_read == 0 || num_rows_read == 1);
  if (num_rows_read == 0) {
    std::cout << "Read Miss: No Key Found" << std::endl; // todo: write out key
    return Status::kNotFound;
  }
  return Status::kOK;
}

Status SpannerDB::Scan(DataTable table,
                      const std::vector<Field> &key,
                      int n,
                      std::vector<TimestampValue> &buffer)
{
  return Status::kNotImplemented;
}

Status SpannerDB::Insert(DataTable table, const std::vector<Field> &key,
                         const TimestampValue & timeval) 
{
  spanner::SqlStatement insert_stmt = table == DataTable::Edges 
      ? GetInsertEdgeSql(key, timeval)
      : GetInsertObjectSql(key, timeval);

  auto commit = info->client.Commit(
    [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
      auto insert = info->client.ExecuteDml(txn, std::move(insert_stmt));
      if (!insert) { return insert.status(); }
      return spanner::Mutations{};
    }
  );
  if (!commit) {
    std::cerr << "Insert operation failed - " << commit.status().message() << std::endl;
    return IsContentionMessage(commit.status().message()) ? Status::kContentionError : Status::kError;
  }
  return Status::kOK;
}

Status SpannerDB::Update(DataTable table, const std::vector<Field> &key,
                         const TimestampValue &timeval) {

  spanner::SqlStatement update_stmt = table == DataTable::Edges
      ? GetUpdateEdgeSql(key, timeval)
      : GetUpdateObjectSql(key, timeval);

  auto commit = info->client.Commit(
    [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
      auto update = info->client.ExecuteDml(txn, std::move(update_stmt));
      if (!update) { return update.status(); }
      return spanner::Mutations{};
    }
  );
  if (!commit) {
    std::cerr << "Update operation failed - " << commit.status().message() << std::endl;
    return IsContentionMessage(commit.status().message()) ? Status::kContentionError : Status::kError;
  }
  return Status::kOK;
}

Status SpannerDB::Delete(DataTable table, const std::vector<Field> &key,
                       const TimestampValue & timeval) {

  spanner::SqlStatement delete_stmt = table == DataTable::Edges
      ? GetDeleteEdgeSql(key, timeval)
      : GetDeleteObjectSql(key, timeval);

  auto commit = info->client.Commit(
    [&] (spanner::Transaction const & txn) -> StatusOr<spanner::Mutations> {
      auto delete_ = info->client.ExecuteDml(txn, std::move(delete_stmt));
      if (!delete_) { return delete_.status(); }
      return spanner::Mutations{};
    }
  );
  if (!commit) {
    std::cerr << "Delete operation failed - " << commit.status().message() << std::endl;
    return IsContentionMessage(commit.status().message()) ? Status::kContentionError : Status::kError;
  }
  return Status::kOK;
}

Status SpannerDB::BatchInsert(DataTable table,
                              const std::vector<std::vector<Field>> &keys,
                              const std::vector<TimestampValue> & timevals)
{
  return table == DataTable::Edges 
      ? BatchInsertEdges(table, keys, timevals)
      : BatchInsertObjects(table, keys, timevals);
}

Status SpannerDB::BatchRead(DataTable table, 
                            std::vector<Field> const & floor_key,
                            std::vector<Field> const & ceiling_key,
                            int n,
                            std::vector<std::vector<Field>> &key_buffer)
{
  auto read_only = spanner::MakeReadOnlyTransaction();
  auto batch_read_stmt = GetBatchReadSql(floor_key, ceiling_key, n);
  auto rows = info->client.ExecuteQuery(read_only, std::move(batch_read_stmt));
  using RowType = std::tuple<int64_t, int64_t, int64_t>;
  int rows_found = 0;
  for (auto const & row : spanner::StreamOf<RowType>(rows)) {
    if (!row) {
      std::cerr << "Invalid row caused scan to fail: " << row.status().message() << std::endl;
      return Status::kError;
    }
    std::vector<Field> row_result = {{"id1", std::get<0>(*row)},
        {"id2", std::get<1>(*row)},
        {"type", std::get<2>(*row)}};
    key_buffer.emplace_back(std::move(row_result));
    ++rows_found;
  }
  if (rows_found == 0) {
    std::cerr << "Scan did not load any rows" << std::endl;
  }
  return Status::kOK;
}

Status SpannerDB::BatchInsertObjects(DataTable table,
                                     const std::vector<std::vector<Field>> &keys,
                                     const std::vector<TimestampValue> &timevals)
{
  auto insert_objects_builder = spanner::InsertMutationBuilder("objects",
      {"id", "timestamp", "value"});
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 1);
    assert(keys[i][0].name == "id");
    insert_objects_builder = insert_objects_builder.EmplaceRow(
        keys[i][0].value,
        timevals[i].timestamp,
        timevals[i].value
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

Status SpannerDB::BatchInsertEdges(DataTable table,
                                   const std::vector<std::vector<Field>> &keys,
                                   const std::vector<TimestampValue> &timevals)
{
  auto insert_edges_builder = spanner::InsertMutationBuilder("edges", 
      {"id1", "id2", "type", "timestamp", "value"});
  for (size_t i = 0; i < keys.size(); ++i) {
    assert(keys[i].size() == 3);
    assert(keys[i][0].name == "id1");
    assert(keys[i][1].name == "id2");
    assert(keys[i][2].name == "type");
    insert_edges_builder = insert_edges_builder.EmplaceRow(
        keys[i][0].value,
        keys[i][1].value,
        keys[i][2].value,
        timevals[i].timestamp,
        timevals[i].value
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

DB *NewSpannerDB() {
    return new SpannerDB;
}

const bool registered = DBFactory::RegisterDB("spanner", NewSpannerDB);

}
