#include "db.h"
#include "edge.h"

namespace benchmark {

  /**
   * Returns a list of keys that are incompatible with the given insertion candidate.
   * The database must ensure that none of these exist in order to insert @param key
   **/
  std::vector<std::vector<DB::Field>> GetIncompatibleKeys(std::vector<DB::Field> const & edge_key) {
    assert(edge_key.size() == 3); // should only run on edges, not objects
    assert(edge_key[0].name == "id1");
    assert(edge_key[1].name == "id2");
    assert(edge_key[2].name == "type");
    int64_t id1 = edge_key[0].value;
    int64_t id2 = edge_key[1].value;
    EdgeType type = static_cast<EdgeType>(edge_key[2].value);
    if (type == EdgeType::Other) {
      /**
       * We are trying to insert the edge (id1, id2, other).
       * This edge assumes (id1, id2) is non-unique, so it is 
       * incompatible with (id1, *, unique) and (id1, *, unique_and_bidirectional).
       * It also assumes (id1, id2) is non-bidirectional, so it is incompatible with
       * (id1, id2, bidirectional) and (id2, id1, *). 
       * Blocking all possible values of a type (represented by *)
       * is done by not including the type
       * in the returned key; "WHERE id1 = a AND id2 = b" is equivalent to 
       * filtering for all possible values of type.
       **/
      return {{{"id1", id1}, {"type", static_cast<int64_t>(EdgeType::Unique)}},
              {{"id1", id1}, {"type", static_cast<int64_t>(EdgeType::UniqueAndBidirectional)}},
              {{"id1", id1}, {"id2", id2}, {"type", static_cast<int64_t>(EdgeType::Bidirectional)}},
              {{"id1", id2}, {"id2", id1}}};
    } else if (type == EdgeType::Bidirectional) {
      /**
       * We are trying to insert the edge (id1, id2, bidirectional).
       * This edge assumes (id1, id2) is non-unique, so it is 
       * incompatible with (id1, *, unique) and (id1, *, unique_and_bidirectional).
       * It also assumes (id1, id2) is bidirectional, so it is incompatible with
       * (id1, id2, other), (id2, id1, other), and (id2, id1, unique), 
       * since those all assume that the edge is not bidirectional. 
       **/
      return {{{"id1", id1}, {"type", static_cast<int64_t>(EdgeType::Unique)}},
              {{"id1", id1}, {"type", static_cast<int64_t>(EdgeType::UniqueAndBidirectional)}},
              {{"id1", id1}, {"id2", id2}, {"type", static_cast<int64_t>(EdgeType::Other)}},
              {{"id1", id2}, {"id2", id1}, {"type", static_cast<int64_t>(EdgeType::Other)}},
              {{"id1", id2}, {"id2", id1}, {"type", static_cast<int64_t>(EdgeType::Unique)}}};
    } else if (type == EdgeType::Unique) {
      /**
       * We are trying to insert the edge (id1, id2, unique).
       * This edge assumes (id1, id2) is unique, so it is 
       * incompatible with any edge starting with id1, in the form (id1, *, *).
       * It also assumes (id1, id2) is non-bidirectional, so it is incompatible with
       * anything in the form (id2, id1, *), since those all imply bidirectionality. 
       **/
      return {{{"id1", id1}},
              {{"id1", id2}, {"id2", id1}}};
    } else if (type == EdgeType::UniqueAndBidirectional) {
      /**
       * We are trying to insert the edge (id1, id2, bidirectional_and_unique).
       * This edge assumes (id1, id2) is unique, so it is 
       * incompatible with any edge starting with id1, in the form (id1, *, *).
       * It also assumes (id1, id2) is non-bidirectional, so it is incompatible with
       * anything in the form (id2, id1, *), since those all imply bidirectionality. 
       **/
      return {{{"id1", id1}},
              {{"id1", id2}, {"id2", id1}, {"type", static_cast<int64_t>(EdgeType::Other)}},
              {{"id1", id2}, {"id2", id1}, {"type", static_cast<int64_t>(EdgeType::Unique)}}};
    } else {
      throw std::invalid_argument("Invalid edge type being inserted: " + EdgeTypeToString(type));
    }
  }

  void PrintResults(std::vector<DB::TimestampValue> const & results) {
    for (auto const & timeval : results) { 
      std::cout << "timestamp=" << timeval.timestamp << ", value=" 
          << timeval.value << std::endl;
    }
  }

  void PrintResults(std::vector<std::vector<DB::Field>> const & results) {
    for (auto const & row : results) {
      for (auto const & field : row) {
        std::cout << field.name << '=' << field.value << ' ';
      }
      std::cout << std::endl;
    }
  }

} // benchmark

