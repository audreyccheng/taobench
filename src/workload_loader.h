#pragma once

#include "db.h"
#include "edge.h"
#include <unordered_map>

namespace benchmark {
  class WorkloadLoader {
  public:

    WorkloadLoader(DB& db, 
                   std::string const & edge_table,
                   std::string const & object_table,
                   std::string const & start_spreader = "", 
                   std::string const & end_spreader = "");

    int WriteToBuffers(int primary_shard,
                        std::string const & primary_key,
                        std::string const & remote_key,
                        std::string const & edge_type,
                        std::string const & timestamp,
                        std::string const & value);

    int LoadFromDB();

    bool FlushEdgeBuffer();

    bool FlushObjectBuffer();

    std::unordered_map<int, std::vector<Edge>> shard_to_edges;

  private:
    DB &db_;
    std::string start_load_spreader_, end_load_spreader_;
    std::string edge_table_, object_table_;
    std::vector<std::vector<DB::Field>> object_key_buffer;
    std::vector<std::vector<DB::Field>> object_val_buffer;
    std::vector<std::vector<DB::Field>> edge_key_buffer;
    std::vector<std::vector<DB::Field>> edge_val_buffer;
  };
}