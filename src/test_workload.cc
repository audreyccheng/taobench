#include "test_workload.h"


namespace benchmark {
  void TestWorkload::Init(DB &db) {
    std::vector<std::vector<DB::Field>> read_results;
    db.Execute({
      "edges",
      {{"id1", "0:23:a"}, {"id2", "1:323:b"}, {"type", "unique"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "e1"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "edges",
      {{"id1", "1:323:b"}, {"id2", "0:78:c"}, {"type", "other"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "e2"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "edges",
      {{"id1", "0:23:a"}, {"id2", "0:78:c"}, {"type", "bidirectional"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "e3"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "edges",
      {{"id1", "4:9:d"}, {"id2", "5:10:e"}, {"type", "other"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "e4"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "objects",
      {{"id", "0:23:a"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o1"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "objects",
      {{"id", "1:323:b"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o2"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "objects",
      {{"id", "0:78:c"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o3"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "objects",
      {{"id", "4:9:d"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o4"}},
      Operation::INSERT
    }, read_results);
    db.Execute({
      "objects",
      {{"id", "5:10:e"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o5"}},
      Operation::INSERT
    }, read_results);
  }

  bool TestWorkload::DoRequest(DB &db) {
    std::vector<std::vector<DB::Field>> before_results;
    db.Execute({
      "objects",
      {{"id", "4:9:d"}},
      {{"timestamp", ""}, {"value", ""}},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      "edges",
      {{"id1", "4:9:d"}, {"id2", "5:10:e",}, {"type", "other"}},
      {{"timestamp", ""}, {"value", ""}},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();
    db.Execute({
      "objects",
      {{"id", "4:9:d"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "o4-n"}},
      Operation::UPDATE
    }, before_results);

    db.Execute({
      "edges",
      {{"id1", "4:9:d"}, {"id2", "5:10:e",}, {"type", "other"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", "e4-n"}},
      Operation::UPDATE
    }, before_results);

    db.Execute({
      "objects",
      {{"id", "4:9:d"}},
      {{"timestamp", ""}, {"value", ""}},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      "edges",
      {{"id1", "4:9:d"}, {"id2", "5:10:e",}, {"type", "other"}},
      {{"timestamp", ""}, {"value", ""}},
      Operation::READ
    }, before_results);

    benchmark::PrintResults(before_results);
    before_results.clear();

    db.Execute({
      "objects",
      {{"id", "4:9:d"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", ""}},
      Operation::DELETE
    }, before_results);

    db.Execute({
      "edges",
      {{"id1", "4:9:d"}, {"id2", "5:10:e",}, {"type", "other"}},
      {{"timestamp", utils::CurrentTimeNanos()}, {"value", ""}},
      Operation::DELETE
    }, before_results);
    return true;
  }
}
