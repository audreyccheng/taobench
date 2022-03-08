#include "test_workload.h"


namespace benchmark {
  void TestWorkload::Init(DB &db) {
    std::vector<DB::TimestampValue> read_results;
    db.Execute({
      DataTable::Edges,
      {{"id1", 0}, {"id2", 1}, {"type", static_cast<int64_t>(EdgeType::Unique)}},
      {utils::CurrentTimeNanos(), "e1"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Edges,
      {{"id1", 1}, {"id2", 2}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {utils::CurrentTimeNanos(), "e2"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Edges,
      {{"id1", 0}, {"id2", 2}, {"type", static_cast<int64_t>(EdgeType::Bidirectional)}},
      {utils::CurrentTimeNanos(), "e3"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Edges,
      {{"id1", 3}, {"id2", 4}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {utils::CurrentTimeNanos(), "e4"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Objects,
      {{"id", 0}},
      {utils::CurrentTimeNanos(), "o1"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Objects,
      {{"id", 1}},
      {utils::CurrentTimeNanos(), "o2"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Objects,
      {{"id", 2}},
      {utils::CurrentTimeNanos(), "o3"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Objects,
      {{"id", 3}},
      {utils::CurrentTimeNanos(), "o4"},
      Operation::INSERT
    }, read_results);
    db.Execute({
      DataTable::Objects,
      {{"id", 4}},
      {utils::CurrentTimeNanos(), "o5"},
      Operation::INSERT
    }, read_results);
  }

  bool TestWorkload::DoRequest(DB &db) {
    std::vector<DB::TimestampValue> before_results;
    db.Execute({
      DataTable::Objects,
      {{"id", 3}},
      {utils::CurrentTimeNanos(), ""},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      DataTable::Edges,
      {{"id1",3}, {"id2", 4}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {utils::CurrentTimeNanos(), ""},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();
    db.Execute({
      DataTable::Objects,
      {{"id", 3}},
      {utils::CurrentTimeNanos(), "o4-n"},
      Operation::UPDATE
    }, before_results);

    db.Execute({
      DataTable::Edges,
      {{"id1", 3}, {"id2", 4,}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {utils::CurrentTimeNanos(), "e4-n"},
      Operation::UPDATE
    }, before_results);

    db.Execute({
      DataTable::Objects,
      {{"id", 3}},
      {0, ""},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      DataTable::Edges,
      {{"id1", 3}, {"id2", 4}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {0, ""},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      DataTable::Objects,
      {{"id", 3}},
      {utils::CurrentTimeNanos(), ""},
      Operation::DELETE
    }, before_results);

    db.Execute({
      DataTable::Edges,
      {{"id1", 3}, {"id2", 4}, {"type", static_cast<int64_t>(EdgeType::Other)}},
      {utils::CurrentTimeNanos(), ""},
      Operation::DELETE
    }, before_results);
    
    return true;
  }
}
