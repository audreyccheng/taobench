
// #include "uniform_generator.h"
// #include "zipfian_generator.h"
// #include "scrambled_zipfian_generator.h"
// #include "skewed_latest_generator.h"
// #include "const_generator.h"
#include "workload.h"
#include "constants.h"
// #include "random_byte_generator.h"

#include <algorithm>
#include <random>
#include <string>

namespace benchmark {

  // Each loader contains a map from primary shard to a list of edges;
  // Returns the combined map
  inline std::unordered_map<int, std::vector<Edge>> CombineKeyMaps(std::vector<std::shared_ptr<WorkloadLoader>> const & loaders)
  {
    std::unordered_map<int, std::vector<Edge>> map;
    for (auto const & loader : loaders) {
      auto & loader_map = loader->shard_to_edges;
      for (auto map_it = loader_map.begin(); map_it != loader_map.end(); ++map_it) {
        std::vector<Edge> & already_mapped = map[map_it->first];
        already_mapped.insert(already_mapped.end(), map_it->second.begin(), map_it->second.end());
      }
      loader_map.clear();
    }
    return map;
  }
  
  TraceGeneratorWorkload::TraceGeneratorWorkload(utils::Properties const & p,
          std::vector<std::shared_ptr<WorkloadLoader>> const & loaders)
      : config_parser(p.GetProperty("config_path"))
      , object_table(p.GetProperty("object_table"))
      , edge_table(p.GetProperty("edge_table"))
      , shard_to_edges(CombineKeyMaps(loaders)) // only used in run phase
  {
    // Check fields were loaded correctly from configs in debug mode.
    assert(config_parser.fields.find("write_txn_sizes") != config_parser.fields.end());
    assert(config_parser.fields.find("operations") != config_parser.fields.end());
    assert(config_parser.fields.find("primary_shards") != config_parser.fields.end());
    assert(config_parser.fields.find("remote_shards") != config_parser.fields.end());
    assert(config_parser.fields.find("edge_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_txn_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("write_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_txn_sizes") != config_parser.fields.end());
    ResizeShardWeights(constants::NUM_SHARDS);
  }

  TraceGeneratorWorkload::TraceGeneratorWorkload(utils::Properties const & p)
      : TraceGeneratorWorkload(p, {})
  {
  }

  void TraceGeneratorWorkload::Init(DB &db) {
    // do nothing, initialization is done in constructor
  }

  long TraceGeneratorWorkload::GetNumKeys(long num_requests) {
    ConfigParser::LineObject & obj = config_parser.fields["write_txn_sizes"];
    long num_keys = 0;
    for (long i = 0; i < num_requests; ++i) {
      num_keys += obj.vals[obj.distribution(rnd::gen)];
    }
    num_keys *= constants::KEY_POOL_FACTOR;
    return num_keys;
  }

  // Given a shard, this function will return a "fake key" that is smaller than every real key on the
  // shard, but larger than any key on the previous shard.
  int64_t TraceGeneratorWorkload::GetShardStartKey(int shard) {
    if (shard < 0 || shard >= constants::NUM_SHARDS) {
      throw std::runtime_error("Invalid spreader passed to GetSpreaderPseudoStartKey");
    }
    return ((int64_t) shard) << 57;
  }

  // Given a shard, this function wil return a "fake key" that is larger than every real key on the
  // shard, but smaller than any key on the next shard.
  int64_t TraceGeneratorWorkload::GetShardEndKey(int shard) {
    if (shard < 0 || shard >= constants::NUM_SHARDS) {
      throw std::runtime_error("Invalid spreader passed to GetSpreaderPseudoEndKey");
    }
    return ((int64_t) (shard+1)) << 57;
  }

  long TraceGeneratorWorkload::GetNumLoadedEdges() {
    long total_size = 0;
    for (auto const & [key, value] : shard_to_edges) {
      total_size += value.size();
    }
    return total_size;
  }

  bool TraceGeneratorWorkload::DoRequest(DB & db) {
    int64_t backoff_limit = constants::INITIAL_BACKOFF_LIMIT_MICROS;
    Status result;
    while ((result = DispatchRequest(db)) == Status::kContentionError) {
      std::uniform_int_distribution<> unif(0, backoff_limit);
      int64_t backoff_micros = unif(rnd::gen);
      std::cerr << "Retrying operation due to contention error; sleep for " << backoff_micros << "us" 
	        << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(backoff_micros));
      backoff_limit = std::max(backoff_limit * 2, backoff_limit); // don't overflow
    }
    return result == Status::kOK;
  }

  Status TraceGeneratorWorkload::DispatchRequest(DB &db) {
    std::discrete_distribution<> op_dist = config_parser.fields["operations"].distribution;
    std::vector<DB::TimestampValue> read_buffer;
    switch (op_dist(rnd::gen)) {
      case 0:
        return db.Execute(GetReadOperation(false), read_buffer);
      case 1:
        return db.Execute(GetWriteOperation(false), read_buffer);
      case 2:
        return db.ExecuteTransaction(GetReadTransaction(), read_buffer, true);
      case 3:
        return db.ExecuteTransaction(GetWriteTransaction(), read_buffer, false);
      default:
        throw std::invalid_argument("Distribution result out of bounds");
    }
  }

  // This function is used in the batch insert phase to generate an edge with new primary and remote keys.
  int TraceGeneratorWorkload::LoadRow(WorkloadLoader &loader) {
    std::uniform_int_distribution<> unif(0, constants::NUM_SHARDS-1);
    ConfigParser::LineObject & remote_shards = config_parser.fields["remote_shards"];
    int primary_shard = unif(rnd::gen);
    int remote_shard = remote_shards.distribution(rnd::gen);
    int64_t primary_key = GenerateKey(primary_shard);
    int64_t remote_key = GenerateKey(remote_shard);
    EdgeType edge_type = GetRandomEdgeType();
    int64_t timestamp = utils::CurrentTimeNanos();
    std::string value = GetValue();
    return loader.WriteToBuffers(primary_shard, primary_key, remote_key, edge_type, timestamp, value);
  }

  void TraceGeneratorWorkload::ResizeShardWeights(int n_shards) {

    // only resize if we need to shrink, otherwise just assume extra shards have weight 0 (?)

    ConfigParser::LineObject & primary_shards = config_parser.fields["primary_shards"];
    ConfigParser::LineObject & remote_shards = config_parser.fields["remote_shards"];

    if (primary_shards.weights.size() > n_shards) {
      std::vector<double> primary_old_weights = std::move(primary_shards.weights);
      primary_shards.weights = std::vector<double>(n_shards);
      double interval = (1.0 * primary_old_weights.size()) / n_shards;
      for (size_t oldi = 0, newi = 0; newi < n_shards; ++newi) {
        double point_mass = 0;
        while ((double) oldi < interval * (newi+1)) {
          point_mass += primary_old_weights[oldi++];
        }
        primary_shards.weights[newi] = point_mass;
      }
      primary_shards.distribution = std::discrete_distribution<>(primary_shards.weights.begin(),
                                                                 primary_shards.weights.end());
    }

    if (remote_shards.weights.size() > n_shards) {
      std::vector<double> remote_old_weights = std::move(primary_shards.weights);
      remote_shards.weights = std::vector<double>(n_shards);
      double interval = (1.0 * remote_old_weights.size()) / n_shards;
      for (size_t oldi = 0, newi = 0; newi < n_shards; ++newi) {
        double point_mass = 0;
        while ((double) oldi < interval * (newi+1)) {
          point_mass += remote_old_weights[oldi++];
        }
        remote_shards.weights[newi] = point_mass;
      }
      remote_shards.distribution = std::discrete_distribution<>(remote_shards.weights.begin(),
                                                                remote_shards.weights.end());
    }
  }

  EdgeType TraceGeneratorWorkload::GetRandomEdgeType() {
    ConfigParser::LineObject & obj = config_parser.fields["edge_types"];
    return EdgeStringToType(obj.types[obj.distribution(rnd::gen)]);
  }

  int64_t TraceGeneratorWorkload::GenerateKey(int shard) {
    int64_t timestamp = utils::CurrentTimeNanos();
    int64_t seqnum = counter::key_count++;
    // 64 bit int split into 7 bit shard, 17 thread-specific sequence number,
    // and bottom 40 bits of timestamp
    // this design is fairly arbitrary; intent is just to minimize duplicate keys across threads
    return (((int64_t) shard) << 57) + ((seqnum & 0x1FFFF) << 40) +
        (timestamp & 0xFFFFFFFFFF);
  }

  std::string TraceGeneratorWorkload::GetRandomReadOperationType(bool is_txn_op) {
    ConfigParser::LineObject & obj = config_parser.fields[is_txn_op ? "read_txn_operation_types"
                                                                    : "read_operation_types"];
    return obj.types[obj.distribution(rnd::gen)];
  }

  std::string TraceGeneratorWorkload::GetRandomWriteOperationType(bool is_txn_op) {
    ConfigParser::LineObject & obj = config_parser.fields[is_txn_op ? "write_txn_operation_types"
                                                                    : "write_operation_types"];
    return obj.types[obj.distribution(rnd::gen)];
  }

  Edge const & TraceGeneratorWorkload::GetRandomEdge() {
    ConfigParser::LineObject & obj = config_parser.fields["primary_shards"];
    auto it = shard_to_edges.find(obj.distribution(rnd::gen));
    while (it == shard_to_edges.end()) {
      it = shard_to_edges.find(obj.distribution(rnd::gen));
    }

    std::uniform_int_distribution<int> edge_selector(0, (it->second).size()-1);
    return (it->second)[edge_selector(rnd::gen)];
  }
  
  std::string TraceGeneratorWorkload::GetValue() {
    std::vector<unsigned char> random_chars(constants::VALUE_SIZE_BYTES);
    std::generate(random_chars.begin(), random_chars.end(), std::ref(rnd::byte_engine));
    for (size_t i = 0; i < constants::VALUE_SIZE_BYTES; ++i) {
      random_chars[i] = 'a' + (random_chars[i] % 26);
    }
    return {random_chars.begin(), random_chars.end()};
  }

  DB::DB_Operation TraceGeneratorWorkload::GetReadOperation(bool is_txn_op) {
    std::string operation_type = GetRandomReadOperationType(is_txn_op);
    bool is_edge_op = operation_type.find("edge") != std::string::npos;
    Edge const & edge = GetRandomEdge();
    if (is_edge_op) {
      return {DataTable::Edges,
               {{"id1", edge.primary_key}, {"id2", edge.remote_key},
                  {"type", static_cast<int64_t>(edge.type)}},
               {0L, ""},
               Operation::READ
             };
    } else {
      return {DataTable::Objects,
               {{"id", edge.primary_key}},
               {0L, ""},
               Operation::READ
             };
    }
  }

  DB::DB_Operation TraceGeneratorWorkload::GetWriteOperation(bool is_txn_op) {
    std::string operation_type = GetRandomWriteOperationType(is_txn_op);
    bool is_edge_op = operation_type.find("edge") != std::string::npos;
    Operation db_op_type;
    // TODO - make these actual values?
    if (operation_type.find("add") != std::string::npos) {
      db_op_type = Operation::INSERT;
    } else if (operation_type == "obj_update" || operation_type == "edge_update") {
      db_op_type = Operation::UPDATE;
    } else if (operation_type == "obj_delete" || operation_type == "edge_delete") {
      db_op_type = Operation::DELETE;
    } else {
      throw std::invalid_argument("Unrecognized write operation");
    }

    Edge edge;
    if (db_op_type != Operation::INSERT) {
      edge = GetRandomEdge();
    } else {
      ConfigParser::LineObject & primary_shards = config_parser.fields["primary_shards"];
      ConfigParser::LineObject & remote_shards = config_parser.fields["remote_shards"];
      edge.primary_key = GenerateKey(primary_shards.distribution(rnd::gen));
      edge.remote_key = GenerateKey(remote_shards.distribution(rnd::gen));
      edge.type = GetRandomEdgeType();
    }
    int64_t timestamp = utils::CurrentTimeNanos();
    std::string value = GetValue();
    if (is_edge_op) {
      return {DataTable::Edges,
               {{"id1", edge.primary_key}, {"id2", edge.remote_key}, {"type", static_cast<int64_t>(edge.type)}},
               {timestamp, std::move(value)},
               db_op_type
             };
    } else {
      return {DataTable::Objects,
               {{"id", edge.primary_key}},
               {timestamp, std::move(value)},
               db_op_type
             };
    }
  }

  std::vector<DB::DB_Operation> TraceGeneratorWorkload::GetReadTransaction() {
    ConfigParser::LineObject & obj = config_parser.fields["read_txn_sizes"];
    int transaction_size = obj.vals[obj.distribution(rnd::gen)];
    std::vector<DB::DB_Operation> ops;
    for (int i = 0; i < transaction_size; ++i) {
      ops.push_back(GetReadOperation(true));
    }
    return ops;
  }

  std::vector<DB::DB_Operation> TraceGeneratorWorkload::GetWriteTransaction() {
    ConfigParser::LineObject & obj = config_parser.fields["write_txn_sizes"];
    int transaction_size = obj.vals[obj.distribution(rnd::gen)];
    std::vector<DB::DB_Operation> ops;
    for (int i = 0; i < transaction_size; ++i) {
      ops.push_back(GetWriteOperation(true));
    }
    return ops;
  }
}
