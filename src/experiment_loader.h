#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace benchmark {
  struct ExperimentInfo {
    
    ExperimentInfo(int threads, long ops, int throughput) 
      : num_threads(threads), num_ops(ops), target_throughput(throughput)
    {
    }

    int num_threads;
    long num_ops;
    int target_throughput;
  };

  inline std::vector<ExperimentInfo> LoadExperiments(std::string const & experiment_path) {
    std::ifstream infile {experiment_path};
    std::vector<ExperimentInfo> loaded_experiments;
    for (std::string line; std::getline(infile, line); ) {
      // skip lines that are commented out
      if (line[0] == '#') {
          continue;
      }
      std::istringstream iss {line};
      int num_threads = 0, target_throughput = 0;
      long num_ops = 0;
      for (int i = 0; i < 3; ++i) {
        std::string token;
        if (!std::getline(iss, token, ',')) {
          throw std::invalid_argument("Experiments config file is not formatted correctly; "
            "each line must be of the format num_threads,num_ops,target_throughput.");
        }
        switch (i) {
          case 0:
            num_threads = std::stoi(token);
            break;
          case 1:
            num_ops = std::stol(token);
            break;
          case 2:
            target_throughput = std::stoi(token);
            break;
        }
      }
      loaded_experiments.emplace_back(num_threads, num_ops, target_throughput);
    }
    return loaded_experiments;
  }

  inline void DescribeExperiments(std::vector<ExperimentInfo> const & experiments) {
    std::cout << "Inputted experiments:" << std::endl;
    for (auto const & experiment : experiments) {
      std::cout << "Running experiment: " << experiment.num_threads << " threads, " 
        << experiment.num_ops << " operations, " << experiment.target_throughput 
        << " ops/sec (targeted)" << std::endl;
    }
  }
}