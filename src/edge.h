
#pragma once
#include <string>

namespace benchmark {
  struct Edge {
    Edge(std::string const & p_key, std::string const & r_key, std::string const & t)
      : primary_key(p_key)
      , remote_key(r_key)
      , type(t)
    {
    }

    Edge(std::string && p_key, std::string && r_key, std::string && t)
      : primary_key(p_key)
      , remote_key(r_key)
      , type(t)
    {
    }
    
    Edge() 
    {
    }

    std::string primary_key;
    std::string remote_key;
    std::string type;
  };
}