#include "ybcql_db.h"
#include "db_factory.h"
#include <cassandra.h>
#include <iostream>
#include <fstream>
#include <openssl/ssl.h>

// namespace {
//     const char* hosts = "cf9bd473-5fcc-4282-935a-37b1a9393e37.aws.ybdb.io";
//     //const char* hosts = "127.0.0.1";
// };

// namespace benchmark {

// void YcqlDB::Init() {
//     // std::lock_guard<std::mutex> lock(mu_);
//     cluster = cass_cluster_new();
//     session = cass_session_new();
//     CassSsl * ssl = cass_ssl_new();
//     if (cass_cluster_set_contact_points(cluster, hosts) != CASS_OK) {
//       std::cerr << "Failed to connect to host" << std::endl;
//     }

//     cass_ssl_set_verify_flags(ssl, CASS_SSL_VERIFY_PEER_CERT);

//     //read the root cert from yugabyte cloud
//     CassError rc;
//     char* cert;
//     long cert_size;

//     FILE* in = fopen("/home/ctin23/research_db/distributed-db-benchmark/root.crt", "rb");
//     if (in == NULL) {
//       fprintf(stderr, "Error loading certificate file '%s'\n", "/home/ubuntu/root.pem");
//       return;
//     }
//     fseek(in, 0, SEEK_END);
//     cert_size = ftell(in);
//     rewind(in);

//     cert = (char*)malloc(cert_size);
//     size_t bytes_read = fread(cert, sizeof(char), cert_size, in);
//     fclose(in);
//     if (bytes_read == (size_t) cert_size) {
//       if (cass_ssl_add_trusted_cert_n(ssl, cert, cert_size) != CASS_OK) {
//         std::cerr << "Failed to set sert" << std::endl;
//       }
//     }
//     free(cert);

//     cass_cluster_set_ssl(cluster, ssl);
//     cass_cluster_set_port(cluster, 9042);
//     cass_cluster_set_credentials(cluster, "admin", "ON&r7Jlkb5aeTMiKqRZWDeShL");
//     cass_ssl_free(ssl);

//     CassFuture* connect_future = cass_session_connect(session, cluster);
//     if (cass_future_error_code(connect_future) != CASS_OK) {
//       const char* message;
//       size_t message_length;
//       cass_future_error_message(connect_future, &message, &message_length);
//       fprintf(stderr, "Unable to connect session: '%.*s'\n", (int)message_length, message);
//     }

//     edge_table_ = "edges";
//     object_table_ = "objects";

//     // Prepare statements
//     read_object_future = cass_session_prepare(session, "SELECT timestamp, value FROM test.objects WHERE id = ?;");
//     read_edge_future = cass_session_prepare(session, "SELECT timestamp, value FROM test.edges WHERE id1 = ? AND id2 = ? AND type = ?;");
//     scan_object_future = cass_session_prepare(session, "SELECT timestamp, value FROM test.objects WHERE id = ? ORDER BY id ASC LIMIT ?;");
//     scan_edge_future = cass_session_prepare(session, "SELECT timestamp, value FROM test.edges WHERE id1 = ? AND id2 = ? AND type = ? ORDER BY id1 ASC, id2 ASC LIMIT ?;");
//     update_object_future = cass_session_prepare(session, "UPDATE test.objects SET timestamp = ?, value = ? WHERE id = ?;");
//     update_edge_future = cass_session_prepare(session, "UPDATE test.edges SET timestamp = ?, value = ? WHERE id1 = ? AND id2 = ? AND type = ? AND timestamp < ?;");
//     insert_object_future = cass_session_prepare(session, "INSERT INTO test.objects (id, timestamp, value) VALUES (?, ?, ?);");
//     insert_bi_future = cass_session_prepare(session, "INSERT INTO test.edges (id1, id2, type, timestamp, value) SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1=? AND type='unique' OR id1=? AND type='unique_and_bidirectional' OR id1=? AND id2=? and type='other' OR id1=? AND id2=? AND type='other' OR id1=? AND id2=? AND type='unique');");
//     insert_other_future = cass_session_prepare(session, "INSERT INTO test.edges (id1, id2, type, timestamp, value) SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1=? AND type='unique' OR id1=? AND type='unique_and_bidirectional' OR id1=? AND id2=? and type='bidirectional' OR id1=? AND id2=?);");
//     insert_unique_future = cass_session_prepare(session, "INSERT INTO test.edges (id1, id2, type, timestamp, value) SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1=? OR id1=? AND id2=?);");
//     insert_biunique_future = cass_session_prepare(session, "INSERT INTO test.edges (id1, id2, type, timestamp, value) SELECT ?, ?, ?, ?, ? WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1=? OR id1=? AND id2=? AND type='other' OR id1=? AND id2=? AND type='unique');");
//     delete_object_future = cass_session_prepare(session, "DELETE FROM test.objects where timestamp<? AND id=?;");
//     delete_edge_future = cass_session_prepare(session, "DELETE FROM test.edges where timestamp<? AND id1=? AND id2=? AND type=?;");
    
// }

// void YcqlDB::Cleanup() {
//   cass_session_free(session);
//   cass_cluster_free(cluster);
// }

// Status YcqlDB::Read(const std::string &table, const std::vector<Field> &key,
//                          const std::vector<std::string> *fields, std::vector<Field> &result) {
//     assert(table == "edges" || table == "objects");
//     assert(fields && fields->size() == 2);
//     assert((*fields)[0] == "timestamp");
//     assert((*fields)[1] == "value");

//     const std::lock_guard<std::mutex> lock(mu_);
//     try {
//       std::cout << "read" << std::endl;
//       // Need to free these after use
//       CassStatement* statement;
//       const CassPrepared* prepared;
//       CassFuture* result_future;
//       const CassResult* r;
//       const CassRow* row;

//       // Binding prepare statements
//       if (table == "objects") {
//         std::cout << "read1" << std::endl;
//         prepared = cass_future_get_prepared(read_object_future);
//         std::cout << "read2" << std::endl;
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id
//       } else {
//         prepared = cass_future_get_prepared(read_edge_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//         cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//         cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//       }

//       // Execute 
//       result_future = cass_session_execute(session, statement);
//       std::cout << "read3" << std::endl;
//       if (cass_future_error_code(result_future) != CASS_OK) {
//         const char* message;
//         size_t message_length;
//         cass_future_error_message(result_future, &message, &message_length);
//         fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//         return Status::kError;
//       }
//       r = cass_future_get_result(result_future);
//       row = cass_result_first_row(r);

//       // Putting result back
//       for (size_t i = 0; i < fields->size(); i++) {
//         const char* value;
//         size_t size = 255;
//         CassError err = cass_value_get_string(cass_row_get_column(row, i), &value, &size);
//         std::string s = value;
//         Field f((*fields)[i], s);
//         result.emplace_back(f);
//       }

//       // Free
//       cass_prepared_free(prepared);
//       cass_statement_free(statement);
//       cass_result_free(r);
//       cass_future_free(result_future);
//       return Status::kOK;
//     }
//     catch (const std::exception &e) {
//       std::cerr << e.what() << std::endl;
//       return Status::kError;
//     }
// }

// Status YcqlDB::Scan(const std::string &table, const std::vector<Field> &key, int len,
//                          const std::vector<std::string> *fields,
//                          std::vector<std::vector<Field>> &result, int start, int end) {
//     // Thread stuffs
//     const std::lock_guard<std::mutex> lock(mu_);

//     try {
//       // Need to free these after use
//       CassStatement* statement;
//       CassFuture* result_future;
//       const CassPrepared* prepared;
//       const CassResult* r;
//       CassIterator* result_iter;

//       // Binding prepare statements
//       if (table == "objects") {
//         prepared = cass_future_get_prepared(scan_object_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id
//         cass_statement_bind_int32(statement, 1, len); //len
//       } else {
//         prepared = cass_future_get_prepared(scan_edge_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//         cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//         cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//         cass_statement_bind_int32(statement, 3, len);
//       }

//       // Execute
//       result_future = cass_session_execute(session, statement);
//       if (cass_future_error_code(result_future) != CASS_OK) {
//         const char* message;
//         size_t message_length;
//         cass_future_error_message(result_future, &message, &message_length);
//         fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//         return Status::kError;
//       }

//       // Putting result back
//       r = cass_future_get_result(result_future);
//       result_iter = cass_iterator_from_result(r);
//       while (cass_iterator_next(result_iter)) {
//         const CassRow* row = cass_iterator_get_row(result_iter);
//         std::vector<Field> rowVector;
//         for (size_t i = 0; i < fields->size(); i++) {
//           const char* value;
//           size_t size = 255;
//           CassError err = cass_value_get_string(cass_row_get_column(row, i), &value, &size);
//           std::string s = value;
//           Field f((*fields)[i], s);
//           rowVector.emplace_back(f);
//         }
//         result.emplace_back(rowVector);
//       }

//       // Free
//       cass_prepared_free(prepared);
//       cass_statement_free(statement);
//       cass_result_free(r);
//       cass_iterator_free(result_iter);
//       cass_future_free(result_future);

//       return Status::kOK;
//     }
//     catch (const std::exception &e) {
//       std::cerr << e.what() << std::endl;
//       return Status::kError;
//     }
// }

// Status YcqlDB::Update(const std::string &table, const std::vector<Field> &key,
//                           const std::vector<Field> &values) {
    
//     assert(table == "edges" || table == "objects");
//     assert(values.size() == 2);
//     assert(values[0].name == "timestamp");
//     assert(values[1].name == "value");
//     const std::lock_guard<std::mutex> lock(mu_);
//     try
//     {
//       // Need to free these after use
//       CassStatement* statement;
//       CassFuture* result_future;
//       const CassPrepared* prepared;

//       // Binding prepare statements
//       if (table == "objects") {
//         std::cout << "upo" << std::endl;
//         prepared = cass_future_get_prepared(update_object_future);
//         std::cout << "upo1" << std::endl;
//         statement = cass_prepared_bind(prepared);
//         std::cout << "upo2" << std::endl;
//         cass_statement_bind_int64(statement, 0, std::stol(values[0].value)); //timestamp
//         cass_statement_bind_string(statement, 1, values[1].value.c_str()); //value
//         cass_statement_bind_string(statement, 2, key[0].value.c_str()); //id
//         cass_statement_bind_int64(statement, 3, std::stol(values[0].value)); //timestamp
//       } else {
//         std::cout << "upe" << std::endl;
//         prepared = cass_future_get_prepared(update_edge_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_int64(statement, 0, std::stol(values[0].value)); //timestamp
//         cass_statement_bind_string(statement, 1, values[1].value.c_str()); //value
//         cass_statement_bind_string(statement, 2, key[0].value.c_str()); //id1
//         cass_statement_bind_string(statement, 3, key[1].value.c_str()); //id2
//         cass_statement_bind_string(statement, 4, key[2].value.c_str()); //type
//         cass_statement_bind_int64(statement, 5, std::stol(values[0].value)); //timestamp
//       }
//       std::cout << "up2" << std::endl;

//       // Execute 
//       result_future = cass_session_execute(session, statement);
//       if (cass_future_error_code(result_future) != CASS_OK) {
//         const char* message;
//         size_t message_length;
//         cass_future_error_message(result_future, &message, &message_length);
//         fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//         return Status::kError;
//       }

//       // Free
//       cass_prepared_free(prepared);
//       cass_statement_free(statement);
//       cass_future_free(result_future);
//       return Status::kOK;
//     }
//     catch (const std::exception &e)
//     {
//       std::cerr << e.what() << std::endl;
//       return Status::kError;
//     }
// }

// Status YcqlDB::Insert(const std::string &table, const std::vector<Field> &key,
//                           const std::vector<Field> &values) {
//     assert(table == "edges" || table == "objects");
//     assert(values.size() == 2);
//     assert(values[0].name == "timestamp");
//     assert(values[1].name == "value");
//     assert(!key.empty());
//     const std::lock_guard<std::mutex> lock(mu_);
//     try
//     {
//       // Need to free these after use
//       CassStatement* statement;
//       CassFuture* result_future;
//       const CassPrepared* prepared;

//       // Binding prepare statements
//       if (table == "objects") {
//         prepared = cass_future_get_prepared(insert_object_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id
//         cass_statement_bind_int64(statement, 1, std::stol(values[0].value)); //timestamp
//         cass_statement_bind_string(statement, 2, values[1].value.c_str()); //value
//       } else {
//         std::string type = key[2].value;
//         if (type == "other") {
//           prepared = cass_future_get_prepared(insert_other_future);
//           statement = cass_prepared_bind(prepared);
//           cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//           cass_statement_bind_int64(statement, 3, std::stol(values[0].value)); //timestamp
//           cass_statement_bind_string(statement, 4, values[1].value.c_str()); //value
//           cass_statement_bind_string(statement, 5, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 6, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 7, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 8, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 9, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 10, key[0].value.c_str()); //id1
//         } else if (type == "bidirectional") {
//           prepared = cass_future_get_prepared(insert_bi_future);
//           statement = cass_prepared_bind(prepared);
//           cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//           cass_statement_bind_int64(statement, 3, std::stol(values[0].value)); //timestamp
//           cass_statement_bind_string(statement, 4, values[1].value.c_str()); //value
//           cass_statement_bind_string(statement, 5, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 6, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 7, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 8, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 9, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 10, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 11, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 12, key[1].value.c_str()); //id2
//         } else if (type == "unique") {
//           prepared = cass_future_get_prepared(insert_unique_future);
//           statement = cass_prepared_bind(prepared);
//           cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//           cass_statement_bind_int64(statement, 3, std::stol(values[0].value)); //timestamp
//           cass_statement_bind_string(statement, 4, values[1].value.c_str()); //value
//           cass_statement_bind_string(statement, 5, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 6, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 7, key[1].value.c_str()); //id2
//         } else if (type == "unique_and_bidirectional") {
//           prepared = cass_future_get_prepared(insert_biunique_future);
//           statement = cass_prepared_bind(prepared);
//           cass_statement_bind_string(statement, 0, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 1, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 2, key[2].value.c_str()); //type
//           cass_statement_bind_int64(statement, 3, std::stol(values[0].value)); //timestamp
//           cass_statement_bind_string(statement, 4, values[1].value.c_str()); //value
//           cass_statement_bind_string(statement, 5, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 6, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 7, key[1].value.c_str()); //id2
//           cass_statement_bind_string(statement, 8, key[0].value.c_str()); //id1
//           cass_statement_bind_string(statement, 9, key[1].value.c_str()); //id2
//         } else {
//           throw std::invalid_argument("Received unknown type");
//         }
//       }

//       // Execute 
//       result_future = cass_session_execute(session, statement);
//       if (cass_future_error_code(result_future) != CASS_OK) {
//         const char* message;
//         size_t message_length;
//         cass_future_error_message(result_future, &message, &message_length);
//         fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//         return Status::kError;
//       }

//       // Free
//       cass_prepared_free(prepared);
//       cass_statement_free(statement); 
//       cass_future_free(result_future);
//       return Status::kOK;
//     }
//     catch (const std::exception &e)
//     {
//       std::cerr << e.what() << std::endl;
//       return Status::kError;
//     }
// }

// Status YcqlDB::BatchInsert(const std::string &table, const std::vector<std::vector<Field>> &keys,
//                              const std::vector<std::vector<Field>> &values) {
//     const std::lock_guard<std::mutex> lock(mu_);
//     assert(table == "edges" || table == "objects");
//     return table == "edges" ? BatchInsertEdges(table, keys, values)
//                             : BatchInsertObjects(table, keys, values);
// }

// Status YcqlDB::BatchInsertObjects(const std::string & table,
//                                    const std::vector<std::vector<Field>> &keys,
//                                    const std::vector<std::vector<Field>> &values) {
//   assert(keys.size() == values.size());
//   assert(!keys.empty());
//   try {
//     CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
//     for (size_t i = 0; i < keys.size(); ++i) {
//       assert(keys[i].size() == 1);
//       assert(keys[i][0].name == "id");
//       assert(values[i].size() == 2);
//       assert(values[i][0].name == "timestamp");
//       assert(values[i][1].name == "value");
//       std::string query = "INSERT INTO test.objects (id, timestamp, value) VALUES ";
//       query += "('" + keys[i][0].value +             // id
//                       "', " + values[i][0].value +          // timestamp
//                       ", '" + values[i][1].value + "');";    // value
      
//       CassStatement* statement = cass_statement_new(query.c_str(), 0);
//       cass_batch_add_statement(batch, statement);
//       cass_statement_free(statement);
//     }

//     CassFuture* result_future = cass_session_execute_batch(session, batch); 
//     if (cass_future_error_code(result_future) != CASS_OK) {
//       const char* message;
//       size_t message_length;
//       cass_future_error_message(result_future, &message, &message_length);
//       fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//       return Status::kError;
//     }
//     cass_future_free(result_future);
//     cass_batch_free(batch);
//     return Status::kOK;
//   } catch (const std::exception &e) {
//     std::cerr << "Batch insert object failed" << e.what() << std::endl;
//     return Status::kError;
//   }
//   return Status::kOK;
// }

// Status YcqlDB::BatchInsertEdges(const std::string & table,
//                                  const std::vector<std::vector<Field>> &keys,
//                                  const std::vector<std::vector<Field>> &values)
// {
//   assert(keys.size() == values.size());
//   assert(!keys.empty());
//   try {
//     CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
//     for (size_t i = 0; i < keys.size(); ++i) {
//       assert(keys[i].size() == 3);
//       assert(keys[i][0].name == "id1");
//       assert(keys[i][1].name == "id2");
//       assert(keys[i][2].name == "type");
//       assert(values[i].size() == 2);
//       assert(values[i][0].name == "timestamp");
//       assert(values[i][1].name == "value");
//       std::string query = "INSERT INTO test.edges (id1, id2, type, timestamp, value) VALUES ";
//       query += "('" + keys[i][0].value +                   // id1
//                       "', '" + keys[i][1].value +           // id2
//                       "', '" + keys[i][2].value +           // type
//                       "', " + values[i][0].value +          // timestamp
//                       ", '" + values[i][1].value + "')";    // value
//       CassStatement* statement = cass_statement_new(query.c_str(), 0);
//       cass_batch_add_statement(batch, statement);
//       cass_statement_free(statement);
//     }
//     CassFuture* result_future = cass_session_execute_batch(session, batch);
//     if (cass_future_error_code(result_future) != CASS_OK) {
//       const char* message;
//       size_t message_length;
//       cass_future_error_message(result_future, &message, &message_length);
//       fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//       return Status::kError;
//     }
//     cass_future_free(result_future);
//     cass_batch_free(batch);
//     return Status::kOK;
//   } catch (const std::exception &e) {
//     std::cerr << "Batch insert edges failed" << e.what() << std::endl;
//     return Status::kError;
//   }
//   return Status::kOK;
// }

// Status YcqlDB::BatchRead(const std::string &table, int start_index, int end_index,
//                          std::vector<std::vector<Field>> &result) {

//   assert(table == "edges");
//   assert(end_index > start_index && start_index >= 0);
//   try {
//     // Need to free these after use
//     CassStatement* statement = cass_statement_new("SELECT id1, id2, type FROM test.edges LIMIT ? OFFSET ?", 2);
//     CassFuture* result_future;
//     const CassResult* r;
//     CassIterator* result_iter;

//     cass_statement_bind_int32(statement, 0, end_index - start_index);
//     cass_statement_bind_int32(statement, 1, start_index);
//     result_future = cass_session_execute(session, statement);  
//     if (cass_future_error_code(result_future) != CASS_OK) {
//       const char* message;
//       size_t message_length;
//       cass_future_error_message(result_future, &message, &message_length);
//       fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//       return Status::kError;
//     }

//     std::vector<std::string> fields;
//     fields.push_back("id1");
//     fields.push_back("id2");
//     fields.push_back("type");
//     r = cass_future_get_result(result_future);
//     result_iter = cass_iterator_from_result(r);
//     while (cass_iterator_next(result_iter)) {
//       const CassRow* row = cass_iterator_get_row(result_iter);
//       std::vector<Field> rowVector;
//       for (size_t i = 0; i < fields.size(); i++) {
//         const char* value;
//         size_t size = 255;
//         CassError err = cass_value_get_string(cass_row_get_column(row, i), &value, &size);
//         std::string s = value;
//         Field f(fields[i], s);
//         rowVector.emplace_back(f);
//     }
//       result.emplace_back(rowVector);
//     }

//     cass_statement_free(statement);
//     cass_result_free(r);
//     cass_iterator_free(result_iter);
//     cass_future_free(result_future);
//     return Status::kOK;
//   } catch (std::exception const &e) {
//     std::cerr << e.what() << std::endl;
//     return Status::kError;
//   }
// }   

// Status YcqlDB::Delete(const std::string &table, const std::vector<Field> &key, const std::vector<Field> &values) {

//     // const std::lock_guard<std::mutex> lock(mu_);
//     try
//     {
//       // Need to free these after use
//       CassStatement* statement;
//       CassFuture* result_future;
//       const CassPrepared* prepared;

//       // Binding prepare statements
//       if (table == "objects") {
//         prepared = cass_future_get_prepared(delete_object_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string_by_name(statement, "timestamp", values[0].value.c_str());
//         cass_statement_bind_string_by_name(statement, "id", key[0].value.c_str());
//       } else {
//         prepared = cass_future_get_prepared(delete_edge_future);
//         statement = cass_prepared_bind(prepared);
//         cass_statement_bind_string_by_name(statement, "timestamp", values[0].value.c_str());
//         cass_statement_bind_string_by_name(statement, "id1", key[0].value.c_str());
//         cass_statement_bind_string_by_name(statement, "id2", key[1].value.c_str());
//         cass_statement_bind_string_by_name(statement, "type", key[2].value.c_str());
//       }

//       // Execute 
//       result_future = cass_session_execute(session, statement);

//       // Free
//       cass_prepared_free(prepared);
//       cass_statement_free(statement);  
//       cass_future_free(result_future);

//       return Status::kOK;
//     }
//     catch (const std::exception &e)
//     {
//       std::cerr << e.what() << std::endl;
//       return Status::kError;
//     }
// }

// Status YcqlDB::Execute(const DB_Operation &operation,
//                            std::vector<std::vector<Field>> &result, bool tx_op) {

//   if(!tx_op) {
//     const std::lock_guard<std::mutex> lock(mu_);  
//   }
//   try {
//     switch (operation.operation) {
//     case Operation::READ: {
//       result.emplace_back();
//       std::vector<std::string> read_fields;
//       for (auto &field : operation.fields) {
//         read_fields.push_back(field.name);
//       }
//       return Read(operation.table, operation.key, &read_fields, result.back());
//     }
//     break;
//     case Operation::INSERT: {
//       return Insert(operation.table, operation.key, operation.fields);
//     }
//     break;
//     case Operation::UPDATE: {
//       return Update(operation.table, operation.key, operation.fields);
//     }
//     break;
//     case Operation::SCAN: {
//       std::vector<std::string> scan_fields;
//       for (auto &field : operation.fields) {
//         scan_fields.push_back(field.name);
//       }
//       return Scan(operation.table, operation.key, operation.len, &scan_fields, result, 0, 1);
//     }
//     break;
//     case Operation::READMODIFYWRITE: {
//       return Status::kNotImplemented;
//     }
//     break;
//     case Operation::DELETE: {
//       return Delete(operation.table, operation.key, operation.fields);
//     }
//     break;
//     case Operation:: MAXOPTYPE: {
//       return Status::kNotImplemented;
//     }
//     break;
//     default:
//       return Status::kNotFound;
//     }
//   } catch (std::exception const &e) {
//     std::cerr << e.what() << std::endl;
//     return Status::kError;
//   }
// }

// Status YcqlDB::ExecuteTransaction(const std::vector<DB_Operation> &operations,
//                                       std::vector<std::vector<Field>> &results, bool read_only) {                     
//   try {
//     std::cout << "executeTran" << std::endl;
//     // Need to free these after use
//     CassStatement* statement;
//     CassFuture* result_future;
//     const CassResult* r;
//     CassIterator* result_iter;
//     // Transaction
//     std::string query = "BEGIN TRANSACTION ";
//     for (const auto &operation : operations) {
//       switch (operation.operation) {
//       case Operation::READ: {
//         std::vector<std::string> read_fields;
//         for (auto &field : operation.fields) {
//           read_fields.push_back(field.name);
//         }
//         query += readQuery(operation.table, operation.key, &read_fields);
//       }
//       break;
//       case Operation::INSERT: {
//         query += insertQuery(operation.table, operation.key, operation.fields);
//       }
//       break;
//       case Operation::UPDATE: {
//         query += updateQuery(operation.table, operation.key, operation.fields);
//       }
//       break;
//       case Operation::SCAN: {
//         std::vector<std::string> scan_fields;
//         for (auto &field : operation.fields) {
//           scan_fields.push_back(field.name);
//         }
//         query += scanQuery(operation.table, operation.key, operation.len, &scan_fields);
//       }
//       break;
//       case Operation::READMODIFYWRITE: {
//         return Status::kNotImplemented;
//       }
//       break;
//       case Operation::DELETE: {
//         query += deleteQuery(operation.table, operation.key, operation.fields);
//       }
//       break;
//       case Operation:: MAXOPTYPE: {
//         return Status::kNotImplemented;
//       }
//       break;
//       default:
//         return Status::kNotFound;
//       }
//     }
//     query += " END TRANSACTION;";
//     statement = cass_statement_new(query.c_str(), 0);

//     // Execute
//     result_future = cass_session_execute(session, statement);  
//     if (cass_future_error_code(result_future) != CASS_OK) {
//       const char* message;
//       size_t message_length;
//       cass_future_error_message(result_future, &message, &message_length);
//       fprintf(stderr, "Unable to run query: '%.*s'\n", (int)message_length, message);
//       return Status::kError;
//     }

//     // Putting result back
//     r = cass_future_get_result(result_future);
//     result_iter = cass_iterator_from_result(r);
//     while (cass_iterator_next(result_iter)) {
//       const CassRow* row = cass_iterator_get_row(result_iter);
//       std::vector<Field> rowVector;
//       for (size_t i = 0; i < operations[0].fields.size(); i++) {
//         const char* value;
//         size_t size = 255;
//         CassError err = cass_value_get_string(cass_row_get_column(row, i), &value, &size);
//         std::string s = value;
//         Field f (operations[0].fields[i].name, s);
//         rowVector.emplace_back(f);
//     }
//       results.emplace_back(rowVector);
//     }

//     // Free
//     cass_statement_free(statement);
//     cass_result_free(r);
//     cass_iterator_free(result_iter);
//     cass_future_free(result_future);

//     return Status::kOK;
//   } catch (std::exception const &e) {
//     std::cerr << e.what() << std::endl;
//     return Status::kError;
//   }
// }

//   std::string YcqlDB::readQuery(const std::string &table, const std::vector<Field> &key,
//                               const std::vector<std::string> *fields) {
//     if (table == "objects") {
//       return " SELECT timestamp, value FROM objects WHERE id = " + key[0].value + ";";
//     } else if (table == "edges") {
//       return " SELECT timestamp, value FROM edges WHERE id1 = " + key[0].value + " AND id2 = " + key[1].value + " AND type = " + key[2].value + ";";
//     } else {
//       throw std::invalid_argument("Received unknown table");
//     }
//   }

//   std::string YcqlDB::scanQuery(const std::string &table, const std::vector<Field> &key, int len,
//                               const std::vector<std::string> *fields) {
//     if (table == "objects") {
//       return " SELECT timestamp, value FROM objects WHERE id = " + key[0].value + " ORDER BY id ASC LIMIT " + std::to_string(len) + ";";
//     } else if (table == "edges") {
//       return " SELECT timestamp, value FROM edges WHERE id1 = " + key[0].value + " AND id2 = " + key[1].value + " AND type = " + key[2].value + " ORDER BY id1 ASC, id2 ASC LIMIT " + std::to_string(len) + ";";
//     } else {
//       throw std::invalid_argument("Received unknown table");
//     }
//   }

//   std::string YcqlDB::updateQuery(const std::string &table, const std::vector<Field> &key,
//                                 const std::vector<Field> &values) {
//     if (table == "objects") {
//       return " UPDATE objects SET timestamp = " + values[0].value + ", value = " + values[1].value + " WHERE id = " + key[0].value + " AND timestamp < " + values[0].value + ";";
//     } else if (table == "edges") {
//       return " UPDATE edges SET timestamp =  " + values[0].value + ", value = " + values[1].value + " WHERE id1 = " + key[0].value + " AND id2 = " + key[1].value + " AND type = " + key[2].value + " AND timestamp < " + values[0].value + ";";
//     } else {
//       throw std::invalid_argument("Received unknown table");
//     }
//   }

//   std::string YcqlDB::insertQuery(const std::string &table, const std::vector<Field> &key,
//                                 const std::vector<Field> &values) {
//     if (table == "objects") {
//       return " INSERT INTO objects (id, timestamp, value) VALUES ("+ key[0].value +", "+ values[0].value +", "+ values[1].value +");";
//     } else if (table == "edges") {
//       std::string type = key[2].value;
//       if (type == "other") {
//         return " INSERT INTO edges (id1, id2, type, timestamp, value) SELECT "+ key[0].value +", "+ key[1].value +", "+ key[2].value +", "+ values[0].value +", "+ values[1].value +
//                " WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1="+ key[0].value +" AND type='unique' OR id1="+ key[0].value +" AND type='unique_and_bidirectional' OR id1="+ key[0].value +
//                " AND id2="+ key[1].value +" and type='bidirectional' OR id1="+ key[0].value +" AND id2="+ key[1].value +");";
//       } else if (type == "bidirectional") {
//         return " INSERT INTO edges (id1, id2, type, timestamp, value) SELECT "+ key[0].value +", "+ key[1].value +", "+ key[2].value +", "+ values[0].value +", "+ values[1].value + 
//                " WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1="+ key[0].value +" AND type='unique' OR id1="+ key[0].value +" AND type='unique_and_bidirectional' OR id1="+ key[0].value +  
//                " AND id2="+ key[1].value +" and type='other' OR id1="+ key[0].value +" AND id2="+ key[1].value +" AND type='other' OR id1="+ key[0].value +" AND id2="+ key[1].value +" AND type='unique');";
//       } else if (type == "unique") {
//         return " INSERT INTO edges (id1, id2, type, timestamp, value) SELECT "+ key[0].value +", "+ key[1].value +", "+ key[2].value +", "+ values[0].value +", "+ values[1].value +
//                " WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1="+ key[0].value +" OR id1="+ key[0].value +" AND id2="+ key[1].value +");";
//       } else if (type == "unique_and_bidirectional") {
//         return " INSERT INTO edges (id1, id2, type, timestamp, value) SELECT "+ key[0].value +", "+ key[1].value +", "+ key[2].value +", "+ values[0].value +", "+ values[1].value +
//                " WHERE NOT EXISTS (SELECT 1 FROM edges WHERE id1="+ key[0].value +" OR id1="+ key[0].value +" AND id2="+ key[1].value +" AND type='other' OR id1="+ key[0].value +" AND id2="+ key[1].value +" AND type='unique');";
//       } else {
//         throw std::invalid_argument("Received unknown type");
//       }
//     } else {
//       throw std::invalid_argument("Received unknown table");
//     }
//   }

//   std::string YcqlDB::deleteQuery(const std::string &table, const std::vector<Field> &key,
//                                 const std::vector<Field> &values) {
//     if (table == "objects") {
//       return "DELETE FROM objects where timestamp < " + key[0].value + " AND id= " + values[0].value + ";";
//     } else if (table == "edges") {
//       return "DELETE FROM edges where timestamp < " + key[0].value + " AND id1=" + key[1].value + " AND id2=" + key[2].value + " AND type=" + values[0].value + ";";
//     } else {
//       throw std::invalid_argument("Received unknown table");
//     }
//   }

// DB *NewYcqlDB() {
//     return new YcqlDB;
// }

// const bool registered = DBFactory::RegisterDB("ybcql", NewYcqlDB);

// } // benchmark
