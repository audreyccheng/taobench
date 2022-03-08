#include "workload_loader.h"
#include "constants.h"

namespace benchmark {

  WorkloadLoader::WorkloadLoader(
        DB& db, 
        int64_t start_key_, 
        int64_t end_key_)
    : db_(db)
    , start_key(start_key_)
    , end_key(end_key_)
  {
  }

  int WorkloadLoader::WriteToBuffers(int primary_shard,
                                     int64_t primary_key,
                                     int64_t remote_key,
                                     EdgeType edge_type,
                                     int64_t timestamp,
                                     std::string const & value)
  {
    int failed_ops = 0;
    shard_to_edges[primary_shard].emplace_back(primary_key, remote_key, edge_type);
    edge_value_buffer.emplace_back(timestamp, value);
    edge_key_buffer.push_back({{"id1", primary_key}, {"id2", remote_key}, {"type", static_cast<int64_t>(edge_type)}});
    object_key_buffer.push_back({{"id", primary_key}});
    object_key_buffer.push_back({{"id", remote_key}});
    object_value_buffer.emplace_back(timestamp, value);
    object_value_buffer.emplace_back(timestamp, value);
    if (edge_value_buffer.size() > constants::WRITE_BATCH_SIZE) {
      failed_ops += FlushEdgeBuffer();
    }
    if (object_value_buffer.size() > constants::WRITE_BATCH_SIZE) {
      failed_ops += FlushObjectBuffer();
    }
    return failed_ops;
  }

  bool WorkloadLoader::FlushEdgeBuffer() {
    bool failed = db_.BatchInsert(DataTable::Edges, edge_key_buffer, edge_value_buffer) != Status::kOK;
    edge_key_buffer.clear();
    edge_value_buffer.clear();
    if (failed) {
      std::cerr << "Warning: Batch insert failed" << std::endl;
    }
    return failed;
  }
  
  bool WorkloadLoader::FlushObjectBuffer() {
    bool failed = db_.BatchInsert(DataTable::Objects, object_key_buffer, object_value_buffer) != Status::kOK;
    object_key_buffer.clear();
    object_value_buffer.clear();
    if (failed) {
      std::cerr << "Warning: Batch insert failed" << std::endl;
    }
    return failed;
  }

  // shard is first 7 bits of id
  inline int GetShardFromKey(int64_t id) {
    return id >> 57;
  }

  int WorkloadLoader::LoadFromDB() {

    int failed_ops = 0;
    int num_read_by_thread = 0;
    // note that the key mapped to id2 is just some placeholder value, the key mapped to id1 
    // will already be less than (for lowest) or greater than (for highest) every 
    // edge that this thread is supposed to load
    std::vector<DB::Field> floor = {{"id1", start_key}, {"id2", 0}, {"type", 0}};
    std::vector<DB::Field> ceiling  = {{"id1", end_key}, {"id2", 0}, {"type", 0}};

    std::vector<DB::Field> last_read;
    std::vector<std::vector<DB::Field>> read_buffer;
    bool is_first = true;
    while (is_first || !last_read.empty()) {
      if (last_read.empty()) {
        if (!is_first) { 
          throw std::runtime_error("Terminal: Batch read failure. DB driver should instead retry until success");
        }
        db_.BatchRead(DataTable::Edges, floor, ceiling, constants::READ_BATCH_SIZE, read_buffer);
        is_first = false;
      } else {
        assert(last_read.size() == 3);
        assert(last_read[0].name == "id1");
        assert(last_read[1].name == "id2");
        assert(last_read[2].name == "type");
        if (db_.BatchRead(DataTable::Edges, last_read, ceiling, 
              constants::READ_BATCH_SIZE, read_buffer) != Status::kOK) {
          throw std::runtime_error("Terminal: Batch read failure. DB driver should instead retry until success. Also valid empty scans should return Status::kOK.");
        }
      }
      num_read_by_thread += read_buffer.size();
      for (auto const & row : read_buffer) {
        assert(row.size() == 3);
        assert(row[0].name == "id1");
        assert(row[1].name == "id2");
        assert(row[2].name == "type");
        shard_to_edges[GetShardFromKey(row[0].value)].emplace_back(
            row[0].value, row[1].value, static_cast<EdgeType>(row[2].value));
      }
      if (read_buffer.empty()) {
        break;
      }
      last_read = read_buffer.back();
      read_buffer.clear();
    }
    std::cout << "num read by thread: " << num_read_by_thread << std::endl;
    return failed_ops;
  }
}

// todo: struct for timestamp / val in db.h