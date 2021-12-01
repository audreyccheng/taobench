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
#include "ybsql_db.h"
#include "experiment_loader.h"

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
      props.SetProperty("total_ops", argv[argindex]);
      argindex++;
    } else if (strcmp(argv[argindex], "-shards") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument for -shards" << std::endl;
        exit(0);
      }
      props.SetProperty("num_shards", argv[argindex]);
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
    } else if (strcmp(argv[argindex], "-rows") == 0) {
      argindex++;
      if (argindex >= argc) {
        UsageMessage(argv[0]);
        std::cerr << "Missing argument for -rows (number of rows)" << std::endl;
        exit(0);
      }
      props.SetProperty("num_rows", argv[argindex]);
      argindex++;
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
      "  -load: run the loading phase of the workload\n"
      "  -t: run the transactions phase of the workload\n"
      "  -run: same as -t\n"
      "  -threads n: number of threads for batch inserts (load) or batch reads (run) (default: 1)\n"
      "  -shards: number of shards to use (default: 50)\n"
      "  -db dbname: specify the name of the DB to use (default: basic)\n"
      "  -P propertyfile: load properties from the given file. Multiple files can\n"
      "                   be specified, and will be processed in the order specified\n"
      "  -C configfile: load workload config from the given file\n"
      "  -E experimentfile: each line gives num_threads, num_ops, and target throughput for an experiment"
      "  -p name=value: specify a property to be passed to the DB and workloads\n"
      "                 multiple properties can be specified, and override any\n"
      "                 values in the propertyfile\n"
      "  -s: print status every 10 seconds (use status.interval prop to override)\n"
      "  -n: number of operations to run (default: 100), only used for load to set load size\n"
      "  -spin: spin on waits rather than sleeping\n"
      "  -rows: number of rows to read in run phase"
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

  // retrieve total number of rows in the edges table
  std::string num_rows;
  if ((num_rows=props.GetProperty("num_rows", "missing")) == "missing") {
      throw std::invalid_argument("Must explicitly provide number of rows for the load phase!");
  }
  int total_num_rows = std::stoi(num_rows);
  int num_rows_per_thread = total_num_rows / num_threads + 1;
  // if we had 21 rows and 4 threads, we'd want first 3 threads to take 6 each,
  // and last one to take 3
  // std::cout << "num rows per thread = " << num_rows_per_thread << std::endl;

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


  std::vector<std::shared_ptr<benchmark::WorkloadLoader>> loaders;
  for (int i = 0, start_index = 0; i < num_threads; ++i, start_index += num_rows_per_thread) {
    int end_for_thread = std::min(start_index + num_rows_per_thread, total_num_rows);
    loaders.push_back(std::make_shared<benchmark::WorkloadLoader>(*dbs[i], edge_table,
        object_table, start_index, end_for_thread));
  }
  std::cout << "loaders" << std::endl;

  std::vector<std::future<int>> batch_read_threads;

  for (int i = 0; i < num_threads; i++) {
    int start = i * 64512 / num_threads;
    int end = (i + 1) * 64512 / num_threads;
    batch_read_threads.emplace_back(std::async(
      std::launch::async,
      benchmark::BatchReadThread,
      loaders[i], start, end
    ));
  }

  int invalid_batch_reads = 0;
  for (auto &n : batch_read_threads) {
    assert(n.valid());
    invalid_batch_reads += n.get();
  }

  // total number of operations to run in workload, not counting insert phase
  const int num_shards = std::stoi(props.GetProperty("num_shards", "50"));
  benchmark::TraceGeneratorWorkload wl {props, num_shards, loaders};

  std::cout << "Number of failed batch reads: " << invalid_batch_reads << std::endl;;
  std::cout << "finished loading!" << std::endl;
  std::cout << "Loaded " << wl.GetNumLoadedEdges() << " edges in total" << std::endl;
  ClearDBs(dbs);

  std::this_thread::sleep_for(std::chrono::seconds(240));

  const bool show_status = (props.GetProperty("status", "true") == "true");
  const int status_interval = std::stoi(props.GetProperty("status.interval", "10"));
  const double warmup_period = std::stod(props.GetProperty("warmup.period", "60.0"));

  benchmark::utils::Timer<double> timer;
  benchmark::utils::Timer<double> warmup_excluded_timer;

  std::string experiment_path;
  if ((experiment_path=props.GetProperty("experiment_path", "missing")) == "missing") {
      throw std::invalid_argument("No experiments path provided!");
  }

  for (benchmark::ExperimentInfo & experiment : experiments) {

    int workload_threads = experiment.num_threads;
    long total_ops = experiment.num_ops;
    int target_throughput = experiment.target_throughput;
    int target_throughput_per_thread = target_throughput / workload_threads;
    std::cout << "Running experiment: " << workload_threads << " threads, " <<
      total_ops << " operations, " << target_throughput << " ops/sec (targeted)" << std::endl;

    std::vector<benchmark::DB *> workload_dbs;
    for (int i = 0; i < workload_threads; i++) {
        benchmark::DB *db = benchmark::DBFactory::CreateDB(&props, &measurements);
        if (db == nullptr) {
            std::cerr << "Unknown database name " << props["dbname"] << std::endl;
            exit(1);
        }
        workload_dbs.push_back(db);
    }

    //std::this_thread::sleep_for(std::chrono::seconds(300));

    CountDownLatch latch(workload_threads);
    measurements.Reset();
    timer.Start();
    warmup_excluded_timer.Start();
    std::future<void> status_future;
    if (show_status) {
      status_future = std::async(std::launch::async, StatusThread,
                                  &measurements, &latch, status_interval, warmup_period,
                                  &warmup_excluded_timer);
    }

    std::vector<std::future<benchmark::ClientThreadInfo>> client_threads;
    for (int i = 0; i < workload_threads; ++i) {
      int thread_ops = total_ops / workload_threads;
      if (i < total_ops % workload_threads) {
        thread_ops++;
      }

      client_threads.emplace_back(std::async(
        std::launch::async,
        benchmark::ClientThread, workload_dbs[i],
        &wl,
        thread_ops,
        target_throughput_per_thread,
        false, // initialize workload, not used rn
        false, // initialize db, we're doing this in CreateDB
        false,  // cleanup db, we do it separately
        !spin, // sleep on waits (vs idling)
        &latch
      ));
    }
    assert((int)client_threads.size() == workload_threads);

    int sum = 0;
    int overtime_sum = 0;
    for (auto &n : client_threads) {
      assert(n.valid());
      benchmark::ClientThreadInfo info = n.get();
      sum += info.completed_ops;
      overtime_sum += info.overtime_ops;
    }
    double runtime = timer.End();
    double warmup_excluded_runtime = warmup_excluded_timer.End();
    uint64_t total_ops_post_warmup = measurements.GetTotalNumOps();

    if (show_status) {
      status_future.wait();
    }


    std::cout << "Total runtime(sec): " << runtime << std::endl;
    std::cout << "Total operations(ops): " << sum << std::endl;
    std::cout << "Overall throughput(ops/sec): " << sum / runtime << std::endl;
    std::cout << "Runtime excluding warmup (sec): " << warmup_excluded_runtime << std::endl;
    std::cout << "Total operations excluding warmup (ops): " << total_ops_post_warmup << std::endl;
    std::cout << "Throughput excluding warmup (ops/sec): " << total_ops_post_warmup/warmup_excluded_runtime << std::endl;
    std::cout << "Number of overtime operations: " << overtime_sum << std::endl;
    std::cout << measurements.GetStatusMsg() << std::endl;
    std::cout << measurements.WriteLatencies() << std::endl;

    ClearDBs(workload_dbs);
    // sleep between experiments
    std::this_thread::sleep_for(std::chrono::seconds(30));
  }
}



void RunLoadPhase(benchmark::utils::Properties & props) {

  std::cout << "Running loading phase!" << std::endl;
  const int num_threads = std::stoi(props.GetProperty("threadcount", "1"));

  std::string object_table = props.GetProperty("object_table", "objects");
  std::string edge_table = props.GetProperty("edge_table", "edges");

  // total number of operations to run in workload, not counting insert phase
  const long total_ops = std::stol(props.GetProperty("total_ops", "100"));
  const int num_shards = std::stoi(props.GetProperty("num_shards", "50"));

  benchmark::Measurements measurements;
  benchmark::TraceGeneratorWorkload wl {props, num_shards};

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
    loaders.push_back(std::make_shared<benchmark::WorkloadLoader>(*dbs[i], edge_table,
        object_table));
  }

  long total_keys = wl.GetNumKeys(total_ops);
  std::cout << total_keys << std::endl;
  long num_keys_per_thread = total_keys / num_threads;

  std::vector<std::future<int>> batch_insert_threads;

  for (int i = 0; i < num_threads; i++) {
    batch_insert_threads.emplace_back(std::async(
      std::launch::async,
      benchmark::BatchInsertThread,
      loaders[i],
      &wl,
      num_keys_per_thread
    ));
  }

  int invalid_batch_inserts = 0;
  for (auto &n : batch_insert_threads) {
    assert(n.valid());
    invalid_batch_inserts += n.get();
  }

  std::cout << "Number of failed batch inserts: " << invalid_batch_inserts << std::endl;
  std::cout << "Done with loading phase!" << std::endl;
}

int main(const int argc, const char *argv[]) {
    benchmark::utils::Properties props;
    ParseCommandLine(argc, argv, props);

    std::cout << "running benchmark!" << std::endl;

    std::string run_phase;
    if ((run_phase=props.GetProperty("run", "missing")) == "missing") {
      throw std::invalid_argument("Must explicitly select run/load phase of workload!");
    }
    bool run = run_phase == "true";

    if (run) {
      RunTransactions(props);
    } else {
      RunLoadPhase(props);
    }
}
