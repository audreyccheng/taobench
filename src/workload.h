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
  thread_local static uint32_t key_count(std::uniform_int_distribution<>(0)(rnd::gen));
}

class Workload {
 public:
  ///
  /// Initialize the scenario.
  /// Called once, in the main client thread, before any operations are started.
  ///
  virtual void Init(DB &db) = 0;

  // Carries out a WorkloadOperation on db.
  virtual bool DoRequest(DB &db) = 0;
};

class TraceGeneratorWorkload : public Workload {
public:

  // This constructor is used for the load (bulk insert) phase.
  TraceGeneratorWorkload(const utils::Properties &p);

  // This constructor is used in the run phase; we combine the workload keypools loaded by each loader.
  TraceGeneratorWorkload(const utils::Properties &p,
                         std::vector<std::shared_ptr<WorkloadLoader>> const & loaders);

  void Init(DB &db) override;

  bool DoRequest(DB &db) override;

  long GetNumKeys(long num_reqs);

  long GetNumLoadedEdges();

private:

  // Deprecated
  void LoadToBuffers(DB& db, std::string const & primary_key,
                     std::string const & remote_key,
                     std::string const & edge_type,
                     std::string const & timestamp,
                     std::string const & value);

public:

  int LoadRow(WorkloadLoader &loader);

  static int64_t GetShardStartKey(int spreader);
  
  static int64_t GetShardEndKey(int spreader);

private:

  Status DispatchRequest(DB &db);

  int64_t GenerateKey(int shard);

  void ResizeShardWeights(int num_shards);

  EdgeType GetRandomEdgeType();

  std::string GetRandomReadOperationType(bool is_txn_op);

  std::string GetRandomWriteOperationType(bool is_txn_op);

  Edge const & GetRandomEdge();

  std::string GetValue();

  DB::DB_Operation GetReadOperation(bool is_txn_op);

  DB::DB_Operation GetWriteOperation(bool is_txn_op);

  std::vector<DB::DB_Operation> GetReadTransaction();

  std::vector<DB::DB_Operation> GetWriteTransaction();

  ConfigParser config_parser;
  std::string const object_table;
  std::string const edge_table;
  std::unordered_map<int, std::vector<Edge>> const shard_to_edges;
};

} // benchmark

#endif // WORKLOAD_H_
