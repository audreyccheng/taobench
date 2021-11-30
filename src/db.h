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
    kNotImplemented
};

///
/// Database interface layer.
/// per-thread DB instance.
///
class DB {
 public:
  struct Field {
    Field(std::string const & name_, std::string const & value_) 
        : name(name_)
        , value(value_)
    {
    }

    Field(std::string && name, std::string && value)
        : name(std::move(name))
        , value(std::move(value))
    {
    }
    
    std::string name;
    std::string value;
  };
  struct DB_Operation {
    DB_Operation(std::string const & t,
                 std::vector<Field> const & k,
                 std::vector<Field> const & f,
                 enum Operation op)
      : table(t)
      , key(k)
      , fields(f)
      , operation(op)
    {
    }
    std::string table;
    std::vector<Field> key;
    std::vector<Field> fields;
    enum Operation operation;
  };

  ///
  /// Initializes any state for accessing this DB.
  ///
  virtual void Init() { }
  ///
  /// Clears any state for accessing this DB.
  ///
  virtual void Cleanup() { }
  ///
  /// Reads a record from the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param table The name of the table.
  /// @param key The list of fields that form the unique key for table.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of field/value pairs for the result.
  /// @return Zero on success, or a non-zero error code on error/record-miss.
  ///
  virtual Status Read(const std::string &table, const std::vector<Field> & key,
                      const std::vector<std::string> *fields,
                      std::vector<Field> &result) = 0;
  ///
  /// Performs a range scan for a set of records in the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param table The name of the table.
  /// @param key The key (as a list of fields comprising the unique key) of last record read; 
  ///        the first key returned is the smallest key greater than this.
  /// @param record_count The number of records to read.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of vectors, where each vector contains field/value
  ///        pairs for one record
  /// @param limit Upper bound for the return value of this scan
  ///        all returned values must have key < limit
  /// @return Zero on success, or a non-zero error code on error.
  ///
  virtual Status Scan(const std::string &table, const std::vector<Field> &key,
                      int record_count, const std::vector<std::string> *fields,
                      std::vector<std::vector<Field>> &result,
                      const std::vector<Field> &limit) = 0;
  ///
  /// Updates a record in the database.
  /// Field/value pairs in the specified vector are written to the record,
  /// overwriting any existing values with the same field names.
  ///
  /// @param table The name of the table.
  /// @param key The key (as a list of fields comprising the unique key) of the record to write.
  /// @param values A vector of field/value pairs to update in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Update(const std::string &table, const std::vector<Field> &key,
                        const std::vector<Field> &values) = 0;
  ///
  /// Inserts a record into the database.
  /// Field/value pairs in the specified vector are written into the record.
  ///
  /// @param table The name of the table.
  /// @param key The key (as a list of fields comprising the unique key) of the record to insert.
  /// @param values A vector of field/value pairs to insert in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Insert(const std::string &table, const std::vector<Field> &key,
                        const std::vector<Field> &values) = 0;


  // Batches multiple row-inserts together; each vector in keys/values should be a row
  // Up to the caller to provide a reasonable batch size.
  // This also does not check for uniqueness/bidirectionality constraints.
  virtual Status BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
                             const std::vector<std::vector<Field>> &values) = 0;

  virtual Status BatchRead(const std::string &table, int start_index, int end_index,
                           std::vector<std::vector<Field>> &result) = 0;


  ///
  /// Deletes a record from the database.
  ///
  /// @param table The name of the table.
  /// @param key The key (as a list of fields comprising the unique key) of the record to delete.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual Status Delete(const std::string &table, const std::vector<Field> &key,
                        const std::vector<Field> &values) = 0;

  /// @param operation DB_operation struct containing table, key fields, Fields, and operation type
  virtual Status Execute(const DB_Operation &operation,
                         std::vector<std::vector<Field>> &result, // for reads
                         bool txn_op = false) = 0;


  /// @param operations vector of operations to be completed as one transaction
  virtual Status ExecuteTransaction(const std::vector<DB_Operation> &operations,
                                    std::vector<std::vector<Field>> &results,
                                    bool read_only) = 0;

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

} // benchmark

#endif // DB_H_
