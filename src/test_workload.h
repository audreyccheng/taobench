#pragma once
#include "workload.h"
#include "db.h"


namespace benchmark {
  class TestWorkload : public Workload {
  public:
    
    TestWorkload() {
      
    }

    void Init(DB &db) override;

    bool DoInsert(DB &db) override {
      return false;
    }

    bool DoRequest(DB &db) override;

  };
}