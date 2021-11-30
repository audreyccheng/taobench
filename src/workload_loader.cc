#include "workload_loader.h"
#include "constants.h"

namespace benchmark {

  WorkloadLoader::WorkloadLoader(
        DB& db, 
        std::string const & edge_table, 
        std::string const & object_table, 
        std::string const & start_spreader, 
        std::string const & end_spreader)
    : db_(db)
    , edge_table_(edge_table)
    , object_table_(object_table)
    , start_load_spreader_(start_spreader)
    , end_load_spreader_(end_spreader)
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
    if (edge_val_buffer.size() > constants::WRITE_BATCH_SIZE) {
      failed_ops += FlushEdgeBuffer();
    }
    if (object_val_buffer.size() > constants::WRITE_BATCH_SIZE) {
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

  // keys are formatted [shard_num]:[loadspreader]:[timestamp]:[sequence_number]
  inline int GetShardFromKey(std::string const & id) {
    size_t colon_pos = id.find(':');
    return std::stoi(id.substr(0, colon_pos));
  }

  int WorkloadLoader::LoadFromDB() {

    int failed_ops = 0;
    int num_read_by_thread = 0;
    // note that the key mapped to id2 is just some placeholder value, the key mapped to id1 
    // will already be less than (for lowest) or greater than (for highest) every 
    // edge that this thread is supposed to load
    std::vector<DB::Field> lowest = {{"id1", start_load_spreader_}, {"id2", "0:0:+"}, {"type", ""}};
    std::vector<DB::Field> highest  = {{"id1", end_load_spreader_}, {"id2", "0:0:+"}, {"type", ""}};

    std::vector<DB::Field> last_read;
    std::vector<std::vector<DB::Field>> read_buffer;
    bool is_first = true;
    while (is_first || !last_read.empty()) {
      if (last_read.empty()) {
        if (!is_first) { 
          throw std::runtime_error("Terminal: Batch read failure. DB driver should instead retry until success");
        }
        db_.Scan(edge_table_, lowest, constants::READ_BATCH_SIZE, {}, read_buffer, highest);
        is_first = false;
      } else {
        assert(last_read.size() == 3);
        assert(last_read[0].name == "id1");
        assert(last_read[1].name == "id2");
        assert(last_read[2].name == "type");
        if (db_.Scan(edge_table_, last_read, constants::READ_BATCH_SIZE,
            {}, read_buffer, highest) != Status::kOK) {
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
            std::move(row[0].value), std::move(row[1].value), std::move(row[2].value));
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