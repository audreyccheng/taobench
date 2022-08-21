#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace benchmark {
  struct ExperimentInfo {
    
    ExperimentInfo(int threads, double warmup_len, double exp_len)
      : num_threads(threads), warmup_len(warmup_len), exp_len(exp_len)
    {
    }

    int num_threads;
    double warmup_len;
    double exp_len;
  };

  // Read experiments.txt file into a vector of ExperimentInfo
  inline std::vector<ExperimentInfo> LoadExperiments(std::string const & experiment_path) {
    std::ifstream infile {experiment_path};
    std::vector<ExperimentInfo> loaded_experiments;
    for (std::string line; std::getline(infile, line); ) {
      // skip lines that are commented out
      if (line[0] == '#') {
          continue;
      }
      std::istringstream iss {line};
      int num_threads = 0;
      double warmup_len = 0;
      double exp_len = 0;
      for (int i = 0; i < 3; ++i) {
        std::string token;
        if (!std::getline(iss, token, ',')) {
          throw std::invalid_argument("Experiments config file is not formatted correctly; "
            "each line must be of the format num_threads,warmup_len,exp_len.");
        }
        switch (i) {
          case 0:
            num_threads = std::stoi(token);
            break;
          case 1:
            warmup_len = std::stod(token);
            break;
          case 2:
            exp_len = std::stod(token);
            break;
        }
      }
      loaded_experiments.emplace_back(num_threads, warmup_len, exp_len);
    }
    return loaded_experiments;
  }

  inline void DescribeExperiments(std::vector<ExperimentInfo> const & experiments) {
    std::cout << "Inputted experiments:" << std::endl;
    for (auto const & experiment : experiments) {
      std::cout << "Running experiment: " << experiment.num_threads << " threads, " 
        << experiment.warmup_len << " seconds (warmup), " << experiment.exp_len << " seconds (experiment)" << std::endl;
    }
  }
}
