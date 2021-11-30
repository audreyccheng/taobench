#include "parse_config.h"

namespace {
  const std::unordered_set<std::string> HAVE_VALS {"write_txn_sizes", "read_txn_sizes"};
  const std::unordered_set<std::string> HAVE_TYPES {"edge_types", "read_operation_types",
        "write_operation_types",
        "read_txn_operation_types", "errors", "txn_errors", "operation_predicates", 
        "txn_predicates", "txn_predicate_counts", "read_tiers", "write_txn_operation_types"};
  const std::unordered_set<std::string> HAVE_NEITHER {"read_operation_latency",                
        "write_operation_latency", "operations",
        "write_txn_latency", "primary_shards", "remote_shards"};
}

namespace benchmark {

  // builds a config parser based on input filepath
  // fields are stored as a map between string names and LineObject objects
  ConfigParser::ConfigParser(std::string const & filepath) {
    std::ifstream infile {filepath};
    for (std::string line; std::getline(infile, line); ) {
      LineObject obj {line};
      fields.emplace(obj.name, std::move(obj));
    }
    // TODO: check that all the desired fields are present
  }

  // print out all line objects for debugging purposes
  void ConfigParser::printOut() const {
    for (auto const& [key, val] : fields) {
      val.printOut();
    }
  }

  // uses a line from the json config file to build a line object
  ConfigParser::LineObject::LineObject(std::string const & line) {
    std::regex rgx("\\{\"name\":\\s*\"(.*)\"(?:,\\s*\"values\":\\s*\\[(.*)\\])?,\\s*\"weights\":\\s*\\[(.*)\\]\\}");
    std::smatch matches;
    bool found_match = std::regex_search(line, matches, rgx);
    assert(found_match);
    assert(matches.size() == 4 || matches.size() == 3); // match for whole line + 2-3 capture groups
    name = matches.str(1);

    if (HAVE_VALS.find(this->name) != HAVE_VALS.end()) {
      vals = parseList<int>(matches.str(2), [] (std::string & s) { return std::stoi(s); });
    } else if (HAVE_TYPES.find(this->name) != HAVE_TYPES.end()) {
      types = parseList<std::string>(matches.str(2), [] (std::string & s) { return s; });
    } else if (HAVE_NEITHER.find(this->name) == HAVE_NEITHER.end()) {
      throw std::invalid_argument("Invalid name read from json: " + this->name);
    }
    weights = parseList<double>(matches.str(3), [] (std::string & s) { return std::stod(s); });
    distribution = std::discrete_distribution<>(weights.begin(), weights.end());
  }

  template<class T>
  std::vector<T> ConfigParser::LineObject::parseList(std::string const & list,
                                                     const std::function<T(std::string&)>& f) {
    std::vector<T> result;
    std::istringstream iss {list};
    std::string token;
    while (std::getline(iss >> std::ws, token, ',')) {
      token.erase(std::remove(token.begin(), token.end(), '"'), token.end());
      result.push_back(f(token));
    }
    return result;
  }

  // print out all fields (for testing)
  void ConfigParser::LineObject::printOut() const {
    std::cout << "Name: " << name << '\n';
    std::cout << "Types: ";
    for (std::string const & type : types) {
      std::cout << type << ' ';
    }
    std::cout << "\nValues: ";
    for (int val : vals) {
      std::cout << val << ' ';
    }
    std::cout << "\nWeights: ";
    for (double weight : weights) {
      std::cout << weight << ' ';
    }
    std::cout << '\n';
  }

  template std::vector<int> ConfigParser::LineObject::parseList<int>(std::string const &, 
          const std::function<int(std::string&)>&);
  template std::vector<double> ConfigParser::LineObject::parseList<double>(std::string const &, 
        const std::function<double(std::string&)>&);
  template std::vector<std::string> ConfigParser::LineObject::parseList<std::string>(std::string const &, const std::function<std::string(std::string&)>&);
}
