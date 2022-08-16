#include <cstring>
#include <ctime>

#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <future>
#include <chrono>
#include <iomanip>

#include "utils.h"
#include "timer.h"
#include "client.h"
#include "measurements.h"
#include "workload.h"
#include "countdown_latch.h"
#include "db_factory.h"
#include "workload.h"
#include "loaders.h"
#include "experiment_loader.h"
#include "constants.h"
#include "test_workload.h"

void ParseCommandLine(int argc, const char *argv[], benchmark::utils::Properties &props);
bool StrStartWith(const char *str, const char *pre);
void UsageMessage(const char *command);

void ParseCommandLine(int argc, const char *argv[], benchmark::utils::Properties &props) {
  int argindex = 1;
  while (argindex < argc && StrStartWith(argv[argindex], "-")) {
    if (strcmp(argv[argindex], "-threads") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -threads" << std::endl;
        exit(0);
      }
      props.SetProperty("threadcount", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-db") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -db" << std::endl;
        exit(0);
      }
      props.SetProperty("dbname", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-P") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -P" << std::endl;
        exit(0);
      }
      std::string filename(argv[argindex]);
      std::ifstream input(argv[argindex]);
      try {
        props.Load(input);
      } catch (const std::string &message) {
        std::cerr << message << std::endl;
        exit(0);
      }
      input.close();
      argindex++;
    } else if (strcmp(argv[argindex], "-C") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -C" << std::endl;
        exit(0);
      }
      props.SetProperty("config_path", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-p") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -p" << std::endl;
        exit(0);
      }
      std::string prop(argv[argindex]);
      size_t eq = prop.find('=');
      if (eq == std::string::npos) {
        std::cerr << "Argument '-p' expected to be in key=value format "
                     "(e.g., -p operationcount=99999)" << std::endl;
        exit(0);
      }
      props.SetProperty(benchmark::utils::Trim(prop.substr(0, eq)),
                        benchmark::utils::Trim(prop.substr(eq + 1)));
      argindex++;
    } else if (strcmp(argv[argindex], "-s") == 0) {
      props.SetProperty("status", "true");
      argindex++;
    } else if (strcmp(argv[argindex], "-n") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument value for -n" << std::endl;
        exit(0);
      }
      props.SetProperty("num_edges", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-E") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument for -E (experimentfile)" << std::endl;
        exit(0);
      }
      props.SetProperty("experiment_path", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-spin") == 0) {
      argindex++;
      props.SetProperty("spin", "true");
    } else if (strcmp(argv[argindex], "-run") == 0 || strcmp(argv[argindex], "-t") == 0) {
      argindex++;
      props.SetProperty("run", "true");
    } else if (strcmp(argv[argindex], "-load") == 0) {
      argindex++;
      props.SetProperty("run", "false");
    } else if (strcmp(argv[argindex], "-test") == 0) {
      argindex++;
      props.SetProperty("test", "true");
    } else {
      UsageMessage(argv[0]);
      std::cerr << "Unknown option '" << argv[argindex] << "'" << std::endl;
      exit(0);
    }
  }

  if (argindex == 1 || argindex != argc) {
    UsageMessage(argv[0]);
    exit(0);
  }
}

namespace OpsCounts {
    uint64_t completed_ops = 0;
    uint64_t failed_ops = 0;
    uint64_t overtime_ops = 0;
}

void StatusThread(benchmark::Measurements *measurements
                  , CountDownLatch *latch
                  , int interval
                  , double warmup_period
                  , benchmark::utils::Timer<double> *timer) {
  // warmup_period is the time (in seconds) we will omit from our measurements
  // to account for database warmup; we will reset measurements once the warmup period
  // is over. this measurement does not have to be precise, we respect the period given
  // to the nearest interval length (default: 10s)
  using namespace std::chrono;
  time_point<system_clock> start = system_clock::now();
  bool done = false;
  bool reset_post_warmup = false;
  while (1) {
    time_point<system_clock> now = system_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);
    duration<double> elapsed_time = now - start;
    if (!reset_post_warmup && elapsed_time.count() > warmup_period) {
      measurements->Reset();
      timer->Start();
      OpsCounts::completed_ops = 0;
      OpsCounts::failed_ops = 0;
      OpsCounts::overtime_ops = 0;
      reset_post_warmup = true;
    }
    std::cout << std::put_time(std::localtime(&now_c), "%F %T") << ' '
              << static_cast<long long>(elapsed_time.count()) << " sec: ";

    std::cout << measurements->GetStatusMsg() << std::endl;

    if (done) {
      break;
    }
    done = latch->AwaitFor(interval);
  };
}

inline bool StrStartWith(const char *str, const char *pre) {
  return strncmp(str, pre, strlen(pre)) == 0;
}

void UsageMessage(const char *command) {
  std::cout <<
      "Usage: " << command << " [options]\n"
      "Options:\n"
      "  -load: run the batch insert phase of the workload\n"
      "  -t: run the transactions phase of the workload\n"
      "  -run: same as -t\n"
      "  -test: run test_workload\n"
      "  -threads n: number of threads for batch inserts (load) or batch reads (run) (default: 1)\n"
      "  -db dbname: specify the name of the DB to use (default: basic)\n"
      "  -P propertyfile: load properties from the given file. Multiple files can\n"
      "                   be specified, and will be processed in the order specified\n"
      "  -C configfile: load workload config from the given file\n"
      "  -E experimentfile: each line gives num_threads, num_ops, and target throughput for an experiment\n"
      "  -p name=value: specify a property to be passed to the DB and workloads\n"
      "                 multiple properties can be specified, and override any\n"
      "                 values in the propertyfile\n"
      "  -s: print status every 10 seconds (use status.interval prop to override)\n"
      "  -n: number of edges in keypool (default: 165 million) to batch insert\n"
      "  -spin: spin on waits rather than sleeping"
      << std::endl;
}

inline void ClearDBs(std::vector<benchmark::DB *> dbs) {
  for (auto db : dbs) {
    db->Cleanup();
    delete db;
  }
  dbs.clear();
}

void RunTransactions(benchmark::utils::Properties & props) {
  const int num_threads = std::stoi(props.GetProperty("threadcount", "1"));

  props.SetProperty("object_table", "objects");
  props.SetProperty("edge_table", "edges");
  std::string object_table = props.GetProperty("object_table", "objects");
  std::string edge_table = props.GetProperty("edge_table", "edges");

  benchmark::Measurements measurements;

  // controls if we spin or sleep when we want to slow down to meet target throughput
  const bool spin = props.GetProperty("spin", "false") == "true";

  // load in experiments from experiment file
  if  (props.GetProperty("experiment_path", "missing") == "missing") {
    throw std::runtime_error("Must specify an experiment file");
  }
  std::vector<benchmark::ExperimentInfo> experiments = benchmark::LoadExperiments(props.GetProperty("experiment_path"));

  std::vector<int> thread_counts {0, num_threads};
  for (auto & experiment : experiments) {
    thread_counts.push_back(experiment.num_threads);
  }

  int max_concurrent_connections = *std::max_element(thread_counts.begin(), thread_counts.end());
  props.SetProperty("max_concurrent_connections", std::to_string(max_concurrent_connections));


  benchmark::DescribeExperiments(experiments);

  // initialize DBs for batch reads
  std::vector<benchmark::DB *> dbs;
  for (int i = 0; i < num_threads; i++) {
      benchmark::DB *db = benchmark::DBFactory::CreateDB(&props, &measurements);
      if (db == nullptr) {
          std::cerr << "Unknown database name " << props["dbname"] << std::endl;
          exit(1);
      }
      dbs.push_back(db);
  }
  std::cout << "finished initializing DBs" << std::endl;


  // we need at most one thread per shard
  if (num_threads > benchmark::constants::NUM_SHARDS) {
    throw std::invalid_argument("Number of threads (" + std::to_string(num_threads)
        + ") must not exceed the number of shards (" + std::to_string(benchmark::constants::NUM_SHARDS));
  }
  int n_shards_per_thread = benchmark::constants::NUM_SHARDS / num_threads;

  // temporary workload object, only used for determining load spreader distribution
  // for batch reads
  benchmark::TraceGeneratorWorkload wl1 {props};

  // Divide up the shards evenly by thread; for each shard,
  // the functions GetShardStartKey and GetShardEndKey return an integer such that
  // the ID1s of all the edges corresponding to shard s lie in the open interval
  // (GetShardStartKey(s), GetShardEndKey(s))
  // Then using these functions and the start/end shards for each thread,
  // we generate start/end points for batch reads from each thread.
  std::vector<std::shared_ptr<benchmark::WorkloadLoader>> loaders;
  for (int i = 0, start_shard = 0; i < num_threads; ++i, start_shard += n_shards_per_thread) {
    int end_for_thread = std::min(start_shard + n_shards_per_thread,
                                  benchmark::constants::NUM_SHARDS);
    if (i >= benchmark::constants::NUM_SHARDS % num_threads) {
      end_for_thread--;
    }
    int64_t start_key = benchmark::TraceGeneratorWorkload::GetShardStartKey(start_shard);
    int64_t end_key = benchmark::TraceGeneratorWorkload::GetShardEndKey(end_for_thread);
    std::cout << "begin: " << start_key << ", end: " << end_key << std::endl;
    loaders.push_back(std::make_shared<benchmark::WorkloadLoader>(*dbs[i], start_key, end_key));
  }
  std::cout << "loaders" << std::endl;

  // Run batch reads in parallel on each thread
  std::vector<std::future<int>> batch_read_threads;

  for (int i = 0; i < num_threads; i++) {
    // int start = i * 64512 / num_threads;
    // int end = (i + 1) * 64512 / num_threads;
    batch_read_threads.emplace_back(std::async(
      std::launch::async,
      benchmark::BatchReadThread,
      loaders[i]
    ));
  }

  int invalid_batch_reads = 0;
  for (auto &n : batch_read_threads) {
    assert(n.valid());
    invalid_batch_reads += n.get();
  }

  // Combine all loaded edges and form workload distributions
  benchmark::TraceGeneratorWorkload wl {props, loaders};

  std::cout << "Number of failed batch reads: " << invalid_batch_reads << std::endl;
  std::cout << "Done with batch read phase!" << std::endl;
  std::cout << "Total edges read: " << wl.GetNumLoadedEdges() << std::endl;
  ClearDBs(dbs);

  std::cout << "Sleeping after batch reads." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(60));

  const bool show_status = (props.GetProperty("status", "true") == "true");
  if (!show_status) {
    throw std::runtime_error("Status thread is needed to clear data from warmup period.");
  }
  const int status_interval = std::stoi(props.GetProperty("status.interval", "10"));
  const double warmup_period = std::stod(props.GetProperty("warmup", std::to_string(benchmark::constants::WARMUP_PERIOD)));

  benchmark::utils::Timer<double> timer;
  benchmark::utils::Timer<double> warmup_excluded_timer;

  std::string experiment_path;
  if ((experiment_path=props.GetProperty("experiment_path", "missing")) == "missing") {
      throw std::invalid_argument("No experiments path provided!");
  }

//  if (std::thread::hardware_concurrency() == 0) {
//    throw std::runtime_error("Compiler does not support std::thread::hardware_concurrency");
//  }

  for (benchmark::ExperimentInfo & experiment : experiments) {
    int num_experiment_threads = experiment.num_threads;
    long num_experiment_ops = experiment.num_ops;
    int target_throughput = experiment.target_throughput;
    int target_throughput_per_thread = target_throughput / num_experiment_threads;
    if (target_throughput_per_thread <= 0) {
      target_throughput_per_thread = 1;
    }
    std::cout << "Running experiment: " << num_experiment_threads << " threads, " <<
      num_experiment_ops << " operations, " << target_throughput << " ops/sec (targeted)" << std::endl;

    std::vector<benchmark::DB *> experiment_dbs;
    for (int i = 0; i < num_experiment_threads; i++) {
        benchmark::DB *db = benchmark::DBFactory::CreateDB(&props, &measurements);
        if (db == nullptr) {
            std::cerr << "Unknown database name " << props["dbname"] << std::endl;
            exit(1);
        }
        experiment_dbs.push_back(db);
    }

    // for TiDB at least, this was needed because connections take time to form
    // might need to adjust
    std::cout << "Sleeping after sending DB connections." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(150));

    CountDownLatch latch(num_experiment_threads);
    measurements.Reset();
    timer.Start();
    OpsCounts::completed_ops = OpsCounts::failed_ops = OpsCounts::overtime_ops = 0;
    warmup_excluded_timer.Start();
    std::future<void> status_future;

    // launch status update thread
    if (show_status) {
      status_future = std::async(std::launch::async, StatusThread,
                                  &measurements, &latch, status_interval, warmup_period,
                                  &warmup_excluded_timer);
    }

    std::vector<std::future<benchmark::ClientThreadInfo>> client_threads;
    for (int i = 0; i < num_experiment_threads; ++i) {
      int thread_ops = num_experiment_ops / num_experiment_threads;
      if (i < num_experiment_ops % num_experiment_threads) {
        thread_ops++;
      }

      client_threads.emplace_back(std::async(
        std::launch::async,
        benchmark::ClientThread, experiment_dbs[i],
        &wl,
        thread_ops,
        i % std::thread::hardware_concurrency(),
        target_throughput_per_thread,
        false, // initialize workload, not used rn
        false, // initialize db, we're doing this in CreateDB
        false,  // cleanup db, we do it separately
        !spin, // sleep on waits (vs idling)
        &latch
      ));
    }
    assert((int)client_threads.size() == num_experiment_threads);

    for (auto &n : client_threads) {
      assert(n.valid());
      benchmark::ClientThreadInfo info = n.get();
      OpsCounts::completed_ops += info.completed_ops;
      OpsCounts::overtime_ops += info.overtime_ops;
      OpsCounts::failed_ops += info.failed_ops;
    }
    double runtime = timer.End();
    double warmup_excluded_runtime = warmup_excluded_timer.End();

    if (show_status) {
      status_future.wait();
    }

    std::cout << "Experiment description: " << num_experiment_threads << " threads, " << 
      num_experiment_ops << " operations, " << target_throughput << " ops/sec (targeted)" << std::endl;
    std::cout << "Total runtime(sec): " << runtime << std::endl;
    std::cout << "Runtime excluding warmup (sec): " << warmup_excluded_runtime << std::endl;
    std::cout << "Total completed operations excluding warmup: " << OpsCounts::completed_ops << std::endl;
    std::cout << "Throughput excluding warmup: " << OpsCounts::completed_ops/warmup_excluded_runtime << std::endl;
    std::cout << "Number of overtime operations excluding warmup: " << OpsCounts::overtime_ops << std::endl;
    std::cout << "Number of failed operations excluding warmup: " << OpsCounts::failed_ops << std::endl;
    std::cout << measurements.GetStatusMsg() << std::endl;
    std::cout << std::endl;

    ClearDBs(experiment_dbs);
    // sleep between experiments
    std::this_thread::sleep_for(std::chrono::seconds(150));
  }
}

void RunBatchInsert(benchmark::utils::Properties & props) {
  std::cout << "Running batch insert phase!" << std::endl;
  const int num_threads = std::stoi(props.GetProperty("threadcount", "1"));

  props.SetProperty("max_concurrent_connections", std::to_string(num_threads));

  std::string object_table = props.GetProperty("object_table", "objects");
  std::string edge_table = props.GetProperty("edge_table", "edges");

  benchmark::Measurements measurements;
  benchmark::TraceGeneratorWorkload wl {props};

  // initialize DBs
  std::vector<benchmark::DB *> dbs;
  for (int i = 0; i < num_threads; i++) {
      benchmark::DB *db = benchmark::DBFactory::CreateDB(&props, &measurements);
      if (db == nullptr) {
          std::cerr << "Unknown database name " << props["dbname"] << std::endl;
          exit(1);
      }
      dbs.push_back(db);
  }
  std::cout << "Created DBs" << std::endl;
  std::vector<std::shared_ptr<benchmark::WorkloadLoader>> loaders;
  for (int i = 0; i < num_threads; ++i) {
    loaders.push_back(std::make_shared<benchmark::WorkloadLoader>(*dbs[i]));
  }

  long total_keys = std::stol(props.GetProperty("num_edges", "165000000"));
  std::cout << total_keys << std::endl;
  long num_keys_per_thread = total_keys / num_threads;

  std::vector<std::future<int>> batch_insert_threads;

  for (int i = 0; i < num_threads; i++) {
    batch_insert_threads.emplace_back(std::async(
      std::launch::async,
      benchmark::BatchInsertThread,
      loaders[i],
      &wl,
      i >= total_keys % num_threads ? num_keys_per_thread : num_keys_per_thread + 1
    ));
  }

  int invalid_batch_inserts = 0;
  for (auto &n : batch_insert_threads) {
    assert(n.valid());
    invalid_batch_inserts += n.get();
  }

  std::cout << "Number of failed batch inserts: " << invalid_batch_inserts << std::endl;
  std::cout << "Done with batch insert phase!" << std::endl;
  ClearDBs(dbs);
}

void RunTestWorkload(benchmark::utils::Properties & props) {
  props.SetProperty("max_concurrent_connections", "1");
  benchmark::Measurements msmnts;
  benchmark::DB *db = benchmark::DBFactory::CreateDB(&props, &msmnts);
  benchmark::TestWorkload twl;
  twl.Init(*db);
  twl.DoRequest(*db);
}

int main(const int argc, const char *argv[]) {
    benchmark::utils::Properties props;
    ParseCommandLine(argc, argv, props);

    std::cout << "running benchmark!" << std::endl;

    bool test = props.GetProperty("test", "false") == "true";
    std::string run_phase;
    if ((run_phase=props.GetProperty("run", "missing")) == "missing" && !test) {
      throw std::invalid_argument("Must explicitly select run/load phase of workload!");
    }
    bool run = run_phase == "true";

    if (run) {
      RunTransactions(props);
    } else if (test) {
      RunTestWorkload(props);
    } else {
      RunBatchInsert(props);
    }
}
