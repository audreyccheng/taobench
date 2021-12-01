#include "workload_loader.h"

namespace {
  const int WRITE_BATCH_SIZE = 1024; // batch size for inserts
  const int READ_BATCH_SIZE = 1024 * 64;
  //const int READ_BATCH_SIZE = 1024 * 64;
}

namespace benchmark {

  WorkloadLoader::WorkloadLoader(
        DB& db, 
        std::string const & edge_table, 
        std::string const & object_table, 
        int start_index, 
        int end_index)
    : db_(db)
    , edge_table_(edge_table)
    , object_table_(object_table)
    , start_row_index_(start_index)
    , end_row_index_(end_index)
  {
  }

  int WorkloadLoader::WriteToBuffers(int primary_shard,
                                      std::string const & primary_key,
                                      std::string const & remote_key,
                                      std::string const & edge_type,
                                      std::string const & timestamp,
                                      std::string const & value)
  {
    int failed_ops = 0;
    shard_to_edges[primary_shard].emplace_back(primary_key, remote_key, edge_type);
    edge_val_buffer.push_back({{"timestamp", timestamp}, {"value", value}});
    edge_key_buffer.push_back({{"id1", primary_key}, {"id2", remote_key}, {"type", edge_type}});
    object_key_buffer.push_back({{"id", primary_key}});
    object_key_buffer.push_back({{"id", remote_key}});
    object_val_buffer.push_back({{"timestamp", timestamp}, {"value", value}});
    object_val_buffer.push_back({{"timestamp", timestamp}, {"value", value}});
    if (edge_val_buffer.size() > WRITE_BATCH_SIZE) {
      failed_ops += FlushEdgeBuffer();
    }
    if (object_val_buffer.size() > WRITE_BATCH_SIZE) {
      failed_ops += FlushObjectBuffer();
    }
    return failed_ops;
  }

  bool WorkloadLoader::FlushEdgeBuffer() {
    bool failed = db_.BatchInsert(edge_table_, edge_key_buffer, edge_val_buffer) != Status::kOK;
    edge_key_buffer.clear();
    edge_val_buffer.clear();
    if (failed) {
      std::cerr << "Warning: Batch insert failed" << std::endl;
    }
    return failed;
  }
  
  bool WorkloadLoader::FlushObjectBuffer() {
    bool failed = db_.BatchInsert(object_table_, object_key_buffer, object_val_buffer) != Status::kOK;
    object_key_buffer.clear();
    object_val_buffer.clear();
    if (failed) {
      std::cerr << "Warning: Batch insert failed" << std::endl;
    }
    return failed;
  }

  // keys are formatted [shard_num]:[timestamp]:[sequence_number]
  inline int GetShardFromKey(std::string const & id) {
    size_t colon_pos = id.find(':');
    return std::stoi(id.substr(0, colon_pos));
  }

  int WorkloadLoader::LoadFromDB(int start, int end) {
    assert(start_row_index_ >= 0);
    assert(end_row_index_ > start_row_index_);
    int failed_ops = 0;
    int next_to_read = start_row_index_;
    std::vector<DB::Field> last_read;
    std::vector<std::vector<DB::Field>> read_buffer;
    bool is_first = true;
    while (next_to_read < end_row_index_) {
      int read_end = std::min(next_to_read + READ_BATCH_SIZE, end_row_index_);
      if (last_read.empty()) {
        if (!is_first) {
          std::cerr << "Scan failed; reading next batch using offset.";
        }
        //if (db_.BatchRead(edge_table_, next_to_read, read_end, read_buffer) != Status::kOK) {
        if (db_.Scan(edge_table_, last_read,read_end-next_to_read, {}, read_buffer, start, end) != Status::kOK) {
          std::cerr << "Batch read failed!" << std::endl;
          failed_ops++;
        }
        is_first = false;
      } else {
        assert(last_read.size() == 3);
        assert(last_read[0].name == "id1");
        assert(last_read[1].name == "id2");
        assert(last_read[2].name == "type");
        if (db_.Scan(edge_table_, last_read,read_end-next_to_read, {}, read_buffer, start, end) != Status::kOK) {
          std::cerr << "Batch read (scan) failed!" << std::endl;
          failed_ops++;
        }
      }
      for (auto const & row : read_buffer) {
        assert(row.size() == 3);
        assert(row[0].name == "id1");
        assert(row[1].name == "id2");
        assert(row[2].name == "type");
        shard_to_edges[GetShardFromKey(row[0].value)].emplace_back(
            std::move(row[0].value), std::move(row[1].value), std::move(row[2].value));
      }
      last_read = read_buffer.back();
      read_buffer.clear();
      next_to_read += READ_BATCH_SIZE;
    }
    return failed_ops;
  }
}