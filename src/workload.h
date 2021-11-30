#ifndef WORKLOAD_H_
#define WORKLOAD_H_

#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <climits>
#include <thread>
#include "db.h"
#include "timer.h"
#include "properties.h"
// #include "generator.h"
// #include "discrete_generator.h"
// #include "counter_generator.h"
// #include "acknowledged_counter_generator.h"
#include "utils.h"
#include "parse_config.h"
#include "workload_loader.h"
#include "edge.h"

namespace benchmark {

namespace rnd {
thread_local static std::mt19937 gen(std::random_device{}());
thread_local static std::independent_bits_engine<std::default_random_engine,
      CHAR_BIT, unsigned char> byte_engine;
}

namespace counter {
  thread_local static int key_count(0);
}

struct LoadSpreaderInfo {

  LoadSpreaderInfo(std::vector<int> const & vals, std::vector<double> const & weights)
    : values(vals), dist(weights.begin(), weights.end())
  {
  }

  std::vector<int> const values;
  std::discrete_distribution<> dist;
};

class Workload {
 public:
  ///
  /// Initialize the scenario.
  /// Called once, in the main client thread, before any operations are started.
  ///
  virtual void Init(DB &db) = 0;
  virtual bool DoInsert(DB &db) = 0;
  virtual bool DoRequest(DB &db) = 0;
};

class TraceGeneratorWorkload : public Workload {
public:

  TraceGeneratorWorkload(const utils::Properties &p, int num_shds);

  TraceGeneratorWorkload(const utils::Properties &p, int num_shds,
                         std::vector<std::shared_ptr<WorkloadLoader>> const & loaders);

  void Init(DB &db) override;

  bool DoInsert(DB &db) override { // setup inserts done by init, workload inserts by DoRequest
    return false;
  }

  bool DoRequest(DB &db) override;

  long GetNumKeys(long num_reqs);

  long GetNumLoadedEdges();

private:

  void LoadToBuffers(DB& db, std::string const & primary_key,
                     std::string const & remote_key,
                     std::string const & edge_type,
                     std::string const & timestamp,
                     std::string const & value);

public:

  int LoadRow(WorkloadLoader &loader);

  std::string GetSpreaderPseudoStartKey(int spreader);
  
  std::string GetSpreaderPseudoEndKey(int spreader);

private:

  std::string GenerateKey(int shard);

  void ResizeShardWeights(int num_shards);

  std::string GetRandomEdgeType();

  std::string GetRandomReadOperationType(bool is_txn_op);

  std::string GetRandomWriteOperationType(bool is_txn_op);

  Edge const & GetRandomEdge();

  std::string GetValue();

  DB::DB_Operation GetReadOperation(bool is_txn_op);

  DB::DB_Operation GetWriteOperation(bool is_txn_op);

  std::vector<DB::DB_Operation> GetReadTransaction();

  std::vector<DB::DB_Operation> GetWriteTransaction();

  ConfigParser config_parser;
  int const num_shards;
  std::string const object_table;
  std::string const edge_table;
  std::unordered_map<int, std::vector<Edge>> const shard_to_edges;

  std::unordered_map<int, LoadSpreaderInfo> shards_to_load_spreader_dists;
  std::unordered_map<int, int> const spreader_to_first_shard;
};

} // benchmark

#endif // WORKLOAD_H_
