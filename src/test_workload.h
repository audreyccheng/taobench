#pragma once
#include "workload.h"
#include "db.h"


namespace benchmark {
  class TestWorkload : public Workload {
  public:
    
    TestWorkload() = default;
    void Init(DB &db) override;
    bool DoRequest(DB &db) override;

  };
}