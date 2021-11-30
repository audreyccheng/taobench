
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

  // each loader contains a map from primary shard to a list of edges;
  // return the combined map
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
  
  /**
   * This function loads the conditional distribution of load spreaders given a shard.
   * 
   * Background: In batch reads, we need multiple threads to read from the edges table at once.
   * Previously, this was done by having the different threads initially seek to the middle of the 
   * table with SQL OFFSET queries, and then read sequentially w/ bookmarking from there.
   * OFFSETs time out on spanner, though, so we adopt the following work-around:
   * 
   * We introduce a new column to the primary key of the objects/edges table (though it's only
   * used for edges) called "load spreaders." These are allocated such that overall, the edges 
   * table has a uniform distribution over the spreaders. The spreaders are "behind" shards and 
   * split across shards as appropriate. For example, if 0 is the primary shard for 90% of edges,
   * and 1-9 each account for 1%, and we have 100 load spreaders, we would allocate spreaders 
   * 0-89 for shard 0 and 90-99 for 1-9. 
   * 
   * This function is building a map that has shards as keys and spreader distributions as values.
   * The LoadSpreaderInfo object contains values corresponding to the spreader numbers associated 
   * with the key shard, and a std::discrete_distribution<> over those values.
   **/
  inline std::unordered_map<int, LoadSpreaderInfo> GenShardLoadSpreaderDists(ConfigParser & c)
  {
    assert(c.fields.find("primary_shards") != c.fields.end());
    ConfigParser::LineObject & obj = c.fields["primary_shards"];
    std::vector<double> probs = obj.distribution.probabilities();

    // compute CDF over distribution of primary shards
    std::vector<double> cdf(probs.size(), 0);
    assert(cdf.size() == probs.size());
    std::partial_sum(probs.begin(), probs.end(), cdf.begin());


    std::unordered_map<int, std::vector<int>> shard_to_spreaders;
    std::unordered_map<int, std::vector<double>> shard_to_weights;

    double mass_per_spreader = 1.0 / constants::NUM_LOAD_SPREADERS;

    for (int spreader = 0; spreader < constants::NUM_LOAD_SPREADERS; ++spreader) {
      double spreader_begin = spreader * mass_per_spreader;
      double spreader_end = (spreader + 1) * mass_per_spreader;
      for (int shard = 0; shard < cdf.size(); ++shard) {
        double shard_begin = shard == 0 ? 0 : cdf[shard-1];
        double shard_end = cdf[shard];
        if (spreader_begin > shard_end || spreader_end < shard_begin) {
          continue;
        } else if (spreader_begin > shard_begin) {
          double end = std::min(spreader_end, shard_end);
          if (end - spreader_begin > 1e-6) {
            shard_to_spreaders[shard].push_back(spreader);
            shard_to_weights[shard].push_back(end - spreader_begin);
          }
        } else if (spreader_end < shard_end) {
          assert(shard_begin >= spreader_begin); // other case should be handled above
          if (spreader_end - shard_begin > 1e-6) {
            shard_to_spreaders[shard].push_back(spreader);
            shard_to_weights[shard].push_back(spreader_end - shard_begin);
          }
        } else {
          // spreader covers entire shard, weight is irrelevant
          assert(spreader_begin < shard_begin && spreader_end > shard_end);
          shard_to_spreaders[shard].push_back(spreader);
          shard_to_weights[shard].push_back(1.0);
        }
      }
    }

    std::unordered_map<int, LoadSpreaderInfo> spreader_dists;
    for (int shard = 0; shard < cdf.size(); ++shard) {
      spreader_dists.try_emplace(shard, shard_to_spreaders[shard], shard_to_weights[shard]);
    }
    assert(spreader_dists.size() == cdf.size());
    return spreader_dists;
  }

  // Given a spreader, return the smallest associated shard.
  // Suppose we wanted to load spreader 7. But because spreaders are the second column of the 
  // primary key, behind primary shards, to find the first row of spreader 7 efficiently 
  // we need to know the first shard mapped with it. Finding the first shard mapped with spreader 8
  // also gives us a (shard, spreader) tuple that is greater than everything mapped to spreader 7
  // to give us an endpoint for the scan.
  inline std::unordered_map<int,int> GetSpreaderToFirstShard(
    std::unordered_map<int, LoadSpreaderInfo> const & shard_to_spreader_dists
  )
  {
    int num_spreaders = constants::NUM_LOAD_SPREADERS;
    std::unordered_map<int, int> spreader_to_first_shard;
    for (int i = 0; i < num_spreaders; ++i) {
      spreader_to_first_shard.emplace(i, shard_to_spreader_dists.size()); // emplace num_shards
    }

    for (auto const & [shard, load_info] : shard_to_spreader_dists) {
      for (int spreader : load_info.values) {
        assert(spreader >= 0 && spreader < num_spreaders);
        spreader_to_first_shard[spreader] = std::min(spreader_to_first_shard[spreader], shard);
      }
    }
    return spreader_to_first_shard;
  }
  
  TraceGeneratorWorkload::TraceGeneratorWorkload(utils::Properties const & p, int num_shds,
          std::vector<std::shared_ptr<WorkloadLoader>> const & loaders)
      : config_parser(p.GetProperty("config_path"))
      , num_shards(num_shds)
      , object_table(p.GetProperty("object_table"))
      , edge_table(p.GetProperty("edge_table"))
      , shard_to_edges(CombineKeyMaps(loaders)) // only used in run phase
      , shards_to_load_spreader_dists(GenShardLoadSpreaderDists(config_parser))
      , spreader_to_first_shard(GetSpreaderToFirstShard(shards_to_load_spreader_dists))
  {
    assert(config_parser.fields.find("write_txn_sizes") != config_parser.fields.end());
    assert(config_parser.fields.find("operations") != config_parser.fields.end());
    assert(config_parser.fields.find("primary_shards") != config_parser.fields.end());
    assert(config_parser.fields.find("remote_shards") != config_parser.fields.end());
    assert(config_parser.fields.find("edge_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_txn_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("write_operation_types") != config_parser.fields.end());
    assert(config_parser.fields.find("read_txn_sizes") != config_parser.fields.end());
    assert(shards_to_load_spreader_dists.size() ==  num_shards);
    ResizeShardWeights(num_shards);
  }

  TraceGeneratorWorkload::TraceGeneratorWorkload(utils::Properties const & p, int num_shds)
      : TraceGeneratorWorkload(p, num_shds, {})
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

  // Given a spreader s, this function will return a key primary_shard:spreader1:id1 that, when 
  // compared as a tuple to (primary_shard, spreader1, id1) in the edges table, will be 
  // 1) Less than every single edge associated with s
  // 2) Greater than every single edge associated with s-1, if s > 0
  std::string TraceGeneratorWorkload::GetSpreaderPseudoStartKey(int spreader) {
    if (spreader < 0 || spreader >= constants::NUM_LOAD_SPREADERS) {
      throw std::runtime_error("Invalid spreader passed to GetSpreaderPseudoStartKey");
    }
    auto const & search_res = spreader_to_first_shard.find(spreader);
    if (search_res == spreader_to_first_shard.end()) {
      throw std::runtime_error("Did not find a shard corresponding to spreader " +
         std::to_string(spreader));
    }
    return std::to_string(search_res->second) + ':' + std::to_string(spreader) + ":+";
  }

  // Given a spreader s, this function will return a key primary_shard:spreader1:id1 that, when 
  // compared as a tuple to (primary_shard, spreader1, id1) in the edges table, will be
  // 1) Greater than every single edge associated with s
  // 2) Less than every single edge associated with s + 1, if s < NUM_LOAD_SPREADERS - 1
  std::string TraceGeneratorWorkload::GetSpreaderPseudoEndKey(int spreader) {
    if (spreader < 0 || spreader >= constants::NUM_LOAD_SPREADERS) {
      throw std::runtime_error("Invalid spreader passed to GetSpreaderPseudoEndKey");
    }
    if (spreader == constants::NUM_LOAD_SPREADERS - 1) {
      return std::to_string(num_shards) + ':' + std::to_string(constants::NUM_LOAD_SPREADERS) + ":+";
    } else {
      return GetSpreaderPseudoStartKey(spreader + 1);
    }
  }

  long TraceGeneratorWorkload::GetNumLoadedEdges() {
    long total_size = 0;
    for (auto const & [key, value] : shard_to_edges) {
      total_size += value.size();
    }
    return total_size;
  }

  bool TraceGeneratorWorkload::DoRequest(DB & db) {
    // field not currently in config file
    std::discrete_distribution<> op_dist = config_parser.fields["operations"].distribution;
    std::vector<std::vector<DB::Field>> read_results;
    switch (op_dist(rnd::gen)) {
      case 0: 
        return db.Execute(GetReadOperation(false), read_results) == Status::kOK;
      case 1:
        return db.Execute(GetWriteOperation(false), read_results) == Status::kOK;
      case 2:
        return db.ExecuteTransaction(GetReadTransaction(), read_results, true) == Status::kOK;
      case 3:
        return db.ExecuteTransaction(GetWriteTransaction(), read_results, false) == Status::kOK;
      default:
        throw std::invalid_argument("Distribution result out of bounds");
    }
    return false;
  }

  int TraceGeneratorWorkload::LoadRow(WorkloadLoader &loader) {
    ConfigParser::LineObject & primary_shards = config_parser.fields["primary_shards"];
    ConfigParser::LineObject & remote_shards = config_parser.fields["remote_shards"];
    int primary_shard = primary_shards.distribution(rnd::gen);
    int remote_shard = remote_shards.distribution(rnd::gen);
    std::string primary_key = GenerateKey(primary_shard);
    std::string remote_key = GenerateKey(remote_shard);
    std::string edge_type = GetRandomEdgeType();
    std::string timestamp = utils::CurrentTimeNanos();
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

  std::string TraceGeneratorWorkload::GetRandomEdgeType() {
    ConfigParser::LineObject & obj = config_parser.fields["edge_types"];
    return obj.types[obj.distribution(rnd::gen)];
  }

  std::string TraceGeneratorWorkload::GenerateKey(int shard) {
    // we need to determine a load spreader for this shard using the spreader distributions
    // marginal distribution of spreaders is uniform over NUM_SPREADERS
    auto search_result = shards_to_load_spreader_dists.find(shard);
    if (search_result == shards_to_load_spreader_dists.end()) {
      throw std::runtime_error("No load spreader distribution found for shard " + std::to_string(shard));
    }
    auto & spreader_info = search_result->second;
    int loadspreader = spreader_info.values[spreader_info.dist(rnd::gen)];
    std::ostringstream thread_id;
    thread_id << std::this_thread::get_id();
    // might not work for larger shard nums
    return std::to_string(shard) + ':' + std::to_string(loadspreader) + ':' + thread_id.str() + ':' + std::to_string(counter::key_count++) + ':' + utils::CurrentTimeNanos();
  }

  std::string TraceGeneratorWorkload::GetRandomReadOperationType(bool is_txn_op) {
    ConfigParser::LineObject & obj = config_parser.fields[is_txn_op ? "read_txn_operation_types"
                                                                    : "read_operation_types"];
    return obj.types[obj.distribution(rnd::gen)];
  }

  std::string TraceGeneratorWorkload::GetRandomWriteOperationType(bool is_txn_op) {
    ConfigParser::LineObject & obj = config_parser.fields["write_operation_types"];
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
      return {edge_table,
               {{"id1", edge.primary_key}, {"id2", edge.remote_key}, {"type", edge.type}},
               {{"timestamp", ""}, {"value", ""}},
               Operation::READ
             };
    } else {
      return {object_table,
               {{"id", edge.primary_key}},
               {{"timestamp", ""}, {"value", ""}},
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
    std::string timestamp = utils::CurrentTimeNanos();
    std::string value = GetValue();
    if (is_edge_op) {
      return {edge_table,
               {{"id1", edge.primary_key}, {"id2", edge.remote_key}, {"type", edge.type}},
               {{"timestamp", std::move(timestamp)}, {"value", std::move(value)}},
               db_op_type
             };
    } else {
      return {object_table,
               {{"id", edge.primary_key}},
               {{"timestamp", std::move(timestamp)}, {"value", std::move(value)}},
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



// std::string Workload::BuildKeyName(uint64_t key_num) {
//   if (!ordered_inserts_) {
//     key_num = utils::Hash(key_num);
//   }
//   std::string prekey = "user";
//   std::string value = std::to_string(key_num);
//   int fill = std::max(0, zero_padding_ - static_cast<int>(value.size()));
//   return prekey.append(fill, '0').append(value);
// }

// void CoreWorkload::BuildValues(std::vector<benchmark::DB::Field> &values) {
//   for (int i = 0; i < field_count_; ++i) {
//     values.push_back(DB::Field());
//     benchmark::DB::Field &field = values.back();
//     field.name.append(field_prefix_).append(std::to_string(i));
//     uint64_t len = field_len_generator_->Next();
//     field.value.reserve(len);
//     RandomByteGenerator byte_generator;
//     std::generate_n(std::back_inserter(field.value), len, [&]() { return byte_generator.Next(); } );
//   }
// }
