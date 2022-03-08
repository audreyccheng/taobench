#pragma once

#include "db.h"
#include "edge.h"
#include <unordered_map>

#include <unordered_map>

namespace benchmark {
  class WorkloadLoader {
  public:

    WorkloadLoader(DB& db, 
                   int64_t start_key = 0, 
                   int64_t end_key = 0);

    int WriteToBuffers(int primary_shard,
                       int64_t primary_key,
                       int64_t remote_key,
                       EdgeType edge_type,
                       int64_t timestamp,
                       std::string const & value);

    int LoadFromDB();

    bool FlushEdgeBuffer();

    bool FlushObjectBuffer();

    std::unordered_map<int, std::vector<Edge>> shard_to_edges;

  private:
    DB &db_;
    int64_t start_key, end_key;
    std::vector<std::vector<DB::Field>> object_key_buffer;
    std::vector<DB::TimestampValue> object_value_buffer;
    std::vector<std::vector<DB::Field>> edge_key_buffer;
    std::vector<DB::TimestampValue> edge_value_buffer;
  };
}