
#pragma once
#include <string>

namespace benchmark {

  enum class EdgeType {
    Unique,
    Bidirectional,
    UniqueAndBidirectional,
    Other
  };

  inline EdgeType EdgeStringToType(std::string const & edge_type) {
    if (edge_type == "unique") {
      return EdgeType::Unique;
    } else if (edge_type == "bidirectional") {
      return EdgeType::Bidirectional;
    } else if (edge_type == "unique_and_bidirectional") {
      return EdgeType::UniqueAndBidirectional;
    } else {
      return EdgeType::Other;
    }
  }

  inline std::string EdgeTypeToString(EdgeType edge_type) {
    switch (edge_type) {
      case EdgeType::Unique:
        return "unique";
      case EdgeType::Bidirectional:
        return "bidirectional";
      case EdgeType::UniqueAndBidirectional:
        return "unique_and_bidirectional";
      default:
        return "other";
    }
  }

  struct Edge {
    Edge(int64_t p_key, int64_t r_key, EdgeType t)
      : primary_key(p_key)
      , remote_key(r_key)
      , type(t)
    {
    }
    
    Edge() 
    {
    }

    int64_t primary_key;
    int64_t remote_key;
    EdgeType type;
  };
}