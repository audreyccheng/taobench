#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <algorithm>
#include <regex>
#include <vector>
#include <random>
#include <iostream>
#include <sstream>
#include <cassert>

namespace benchmark {
  class ConfigParser {

    public:

      ConfigParser(std::string const & configFilepath);
      void printOut() const; // for testing

      struct LineObject {
        LineObject(std::string const & line);

        LineObject() {
          throw std::invalid_argument("Attempted to construct a line object that does not exist!");
        }

        template <class T>
        std::vector<T> parseList(std::string const & list, 
                                 const std::function<T(std::string&)>& f);

        void printOut() const; // for testing

        std::string name;
        std::vector<std::string> types;
        std::vector<int> vals;
        std::vector<double> weights;
        std::discrete_distribution<> distribution;
      };

      std::unordered_map<std::string, LineObject> fields;
  };
}