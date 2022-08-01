#ifndef DB_H_
#define DB_H_

#include "properties.h"

#include <vector>
#include <string>
#include <iostream>

namespace benchmark {

enum class Operation {
  INSERT,
  READ,
  UPDATE,
  SCAN,
  READMODIFYWRITE,
  DELETE,
  READTRANSACTION,
  WRITETRANSACTION,
  MAXOPTYPE,
};

enum class Status {
    kOK,
    kError,
    kNotFound,
    kNotImplemented,
    kContentionError
};

enum class DataTable {
    Edges,
    Objects
  };

inline std::string DataTableToStr(DataTable table) {
  switch (table) {
    case DataTable::Edges:
      return "edges";
    case DataTable::Objects:
      return "objects";
    default:
      throw std::invalid_argument("Invalid datatable type");
  }
}

///
/// Database interface layer.
/// per-thread DB instance.
///
class DB {
 public:

  struct Field {
    Field(std::string const & name_, int64_t value_) 
        : name(name_)
        , value(value_)
    {
    }

    std::string name;
    int64_t value;
  };

  struct TimestampValue {
    TimestampValue(int64_t timestamp_, std::string const & value_)
      : timestamp(timestamp_)
      , value(value_)
    {

    }

    int64_t timestamp;
    std::string value;
  };

  struct DB_Operation {

    DB_Operation(DataTable tab, std::vector<Field> const & k, TimestampValue const & timeval, Operation op)
      : table(tab)
      , key(k)
      , time_and_value(timeval)
      , operation(op)
    {
    }

    DataTable table;
    std::vector<Field> key; // 1 int for objects, 3 (id1, id2, type) for edge
    TimestampValue time_and_value;
    Operation operation;
  };
  


  /// Initializes any state for accessing this DB.
  ///
  virtual void Init() { }


  /// Clears any state for accessing this DB.
  ///
  virtual void Cleanup() { }


  /// Reads a record from the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param table DataTable::Edges or DataTable::Objects
  /// @param key Key being read.
  ///            This vector will either have 3 elements in the format {{"id1", @id1}, {"id2", @id2}, {"type", @type}}
  ///            or one element in the format {{"id", @id}} for edges and objects, respectively.
  ///            All of the parameters (including type) are 64-bit ints.
  /// @param buffer A vector of timestamp/value pairs. This function should append one value to this.
  /// @return Zero on success, or a non-zero error code on error/record-miss.
  ///
  virtual Status Read(DataTable table, const std::vector<Field> & key,
                      std::vector<TimestampValue> &buffer) = 0;


  /// NOTE: this function is meant to support future SCAN operations in the workload. 
  /// It is NOT used for batch reads.
  /// This function reads the @param n smallest rows greater than or equal to @param key.
  /// and writes them to @param buffer in sorted order.
  /// The timestamp/value pairs from these rows should be appended to buffer.
  /// As before, @param table specifies the table being read. Format of @param key
  /// is identical to reads.
  virtual Status Scan(DataTable table, const std::vector<Field> & key, int n,
                      std::vector<TimestampValue> &buffer) = 0;


  /// Updates a record in @param table for @param key with @param value
  ///
  /// @param table DataTable::Edges or DataTable::Objects
  /// @param key Key being updated, formatted as in the Read method
  /// @param value Timestamp/Value pair specifying new value for the row.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Update(DataTable table, const std::vector<Field> &key,
                        TimestampValue const & value) = 0;


  /// Inserts a record in @param table for @param key with @param value.
  /// Argument formatting identical to Update.
  virtual Status Insert(DataTable table, const std::vector<Field> &key,
                        TimestampValue const & value) = 0;


  /// Deletes a record from the database.
  ///
  /// @param table The name of the table.
  /// @param key The key (as a list of fields comprising the unique key) of the record to delete.
  /// @param value - key should only be deleted if its associated timestamp is less than value.timestamp
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Delete(DataTable table, const std::vector<Field> &key,
                        TimestampValue const & value) = 0;


  /// Execute a single operation (READ, INSERT, UPDATE, DELETE) TODO: maybe add SCAN here?
  /// @param operation DB_operation struct containing table, key, value, and operation type
  /// @param read_buffer - append read result here if applicable
  virtual Status Execute(const DB_Operation &operation,
                         std::vector<TimestampValue> &read_buffer, // for reads
                         bool txn_op = false) = 0;


  /// @param operations vector of operations to be completed as one transaction
  virtual Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                    std::vector<TimestampValue> &read_buffer,
                                    bool read_only) = 0;


  /// Insert records in @param table for @param keys with @param values
  /// Each element of @param keys is a key vector, formatted as in the other operations
  /// @param values is a vector of TimestampValue pairs
  /// Value for the ith key (ith element of @param keys) will be the ith element of @param values
  virtual Status BatchInsert(DataTable table, const std::vector<std::vector<Field>> &keys,
                             std::vector<TimestampValue> const & values) = 0;

  /// NOTE: The main difference between this method and Scan is that this reads keys, not values.
  /// This method reads the first @n keys (or all of them, whichever is smaller) from @param table
  /// in the OPEN interval ( @param floor_key, @param ceiling_key) and writes them to @param key_buffer
  /// in sorted order. Key formatting is identical to previous methods.
  ///
  /// @param table DataTable::Edges; this is never called on Objects table
  /// @param floor_key - First key read should be the smallest key strictly greater than this.
  ///                  Formatting for edges: {{"id1", @id1}, {"id2", @id2}, {"type", @type}}
  /// @param ceiling_key - All keys read must be strictly less than this. The batch read should stop
  /// when it hits the @param ceiling_key or when it has read @param n elements, whichever comes sooner.
  /// @param result A sorted vector of keys (which are vectors of Fields, formatted as above) read by this scan.
  /// The result should be ordered by (id1, id2, type); all 3 of these fields are ints
  /// @return Zero on success, or a non-zero error code on error.
  ///
  virtual Status BatchRead(DataTable table, const std::vector<Field> &floor_key,
                           const std::vector<Field> &ceiling_key,
                           int n, std::vector<std::vector<Field>> &key_buffer) = 0;


  virtual ~DB() { }

  void SetProps(utils::Properties *props) {
    props_ = props;
  }
 protected:
  utils::Properties *props_;
};

/**
 * Returns a list of keys that are incompatible with the given insertion candidate @param key.
 * The database must ensure that none of these exist in order to insert @param key.
 **/
std::vector<std::vector<DB::Field>> GetIncompatibleKeys(std::vector<DB::Field> const & key);

// Prints out results vector to stdout
void PrintResults(std::vector<std::vector<DB::Field>> const & results);

void PrintResults(std::vector<DB::TimestampValue> const & results);

} // benchmark

#endif // DB_H_
