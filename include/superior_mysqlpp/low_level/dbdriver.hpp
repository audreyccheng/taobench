/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <mysql/mysql.h>
#include <pthread.h>

#include <stdexcept>
#include <mutex>

#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <type_traits>
#include <cinttypes>
#include <atomic>
#include <cstring>

#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/exceptions.hpp>
#include <superior_mysqlpp/utils.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/low_level/mysql_hacks.hpp>


// MariaDB connector/C is supported since version 10.2.
// MARIADB_VERSION_ID doesn't exist in older versions, MARIADB_PACKAGE_VERSION is used to detect MariaDB presence instead.
#if defined(MARIADB_PACKAGE_VERSION) && (!defined(MARIADB_VERSION_ID) || MARIADB_VERSION_ID < 100200)
    #error Older versions of MariaDB connector/C are not supported upto version 10.2 (see README.md for details).
#endif

/*
 * Layer directly communicating with underlying MySQL C API.
 *
 * It should be the only place in code communicating to MySQL library functions!
 * C API header file mysql/mysql.h might be also included elsewhere for providing
 * internal MySQL types like MYSQL_BIND, MYSQL_ROW and other structures.
 *
 * For complete list of C API methods, see:
 * https://dev.mysql.com/doc/refman/5.7/en/c-api-function-overview.html
 */

namespace SuperiorMySqlpp { namespace LowLevel
{
    namespace detail
    {
        /**
         * MySQL library initialization class implemenented as singleton.
         * Call method #initialize() to initialize underlying MySQL library.
         */
        class MysqlLibraryInitWrapper
        {
        private:
            /**
             * MySQL client library initialization.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-library-init.html
             *
             * @throws MysqlInternalError When any error occurred during library initialization.
             */
            MysqlLibraryInitWrapper()
            {
                if (mysql_library_init(0, nullptr, nullptr))
                {
                    throw MysqlInternalError("Could not initialize MYSQL library!");
                }
            }

            /**
             * MySQL client library finalization and memory cleanup.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-library-end.html
             */
            ~MysqlLibraryInitWrapper()
            {
                mysql_library_end();
            }

        public:
            /**
             * Thread-safe initialization of underlying MySQL library.
             */
            static void initialize()
            {
                // Thread-safe initialization
                static MysqlLibraryInitWrapper instance{};
            }
        };
    }


    /**
     * Class holding information about connection to MySQL server.
     * Suitable for using in threads.
     */
    class DBDriver
    {
    private:
        /** Internal ID; each instance have unique ID. 0 means invalid/not connected state */
        std::uint_fast64_t id;
        /** Connection handler settings. */
        MYSQL mysql;
        /** Logger instance pointer. */
        Loggers::SharedPointer_t loggerPtr;

        using size_t = unsigned long long;

    private:
        /**
         * Internal thread-safe ID counter.
         * @return ID counter reference.
         */
        static auto& getGlobalIdRef() noexcept
        {
            static std::atomic<std::uint_fast64_t> globalId{1};
            return globalId;
        }

        /**
         * Returns current MySQL handler settings.
         * @return Reference to internal MYSQL structure.
         */
        MYSQL& getMysql() noexcept
        {
            return mysql;
        }

        /**
         * Returns current MySQL handler settings.
         * @return Pointer to internal MYSQL structure.
         */
        MYSQL* getMysqlPtr() noexcept
        {
            return &mysql;
        }

        /**
         * Initializes library and allocates MySQL connection handler.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-init.html
         *
         * @throws MysqlInternalError When any error occurred during initialization.
         */
        void mysqlInit()
        {
            detail::MysqlLibraryInitWrapper::initialize();

            if (mysql_init(getMysqlPtr()) == nullptr)
            {
                throw MysqlInternalError("Could not initialize MYSQL library. (mysql_init failed)");
            }

            if (mysql_thread_safe())
            {
                detail::MySqlThreadRaii::setup();
            }
        }

        /**
         * Closes connection and deallocates MySQL connection handler.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-close.html
         */
        void mysqlClose()
        {
            if (isConnected())
            {
                getLogger()->logMySqlClose(id);
                id = 0;
            }
            mysql_close(getMysqlPtr());
        }


    public:
        /**
         * Initialize underlying library and internal structures.
         * @param loggerPtr Custom logger (optional).
         */
        DBDriver(Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : id{0}, loggerPtr{std::move(loggerPtr)}
        {
            mysqlInit();
        }

        /**
         * Close connection and cleanup all acquired resources.
         */
        ~DBDriver()
        {
            mysqlClose();
        }

        DBDriver(const DBDriver&) = delete;
        DBDriver& operator=(const DBDriver&) = delete;

        DBDriver(DBDriver&& drv)
            : id{drv.id}, mysql(drv.mysql), loggerPtr{drv.loggerPtr}
        {
            drv.id = 0;
            drv.mysqlInit();
        }

        DBDriver& operator=(DBDriver&&) = delete;


        /**
         * Returns current logger instance.
         * @return Shared pointer to logger instance.
         */
        auto& getLoggerPtr() noexcept
        {
            return loggerPtr;
        }

        /**
         * Returns current logger instance.
         * @return Constant shared pointer to logger instance.
         */
        const auto& getLoggerPtr() const noexcept
        {
            return loggerPtr;
        }

        /**
         * Sets logger instance at runtime.
         * @remark Universal (forwarding) reference covering perfect forwarding for l-value and r-value references.
         *
         * @tparam T Shared pointer of any Logger::Interface implementation.
         * @param value Logger instance.
         */
        template<typename T>
        void setLoggerPtr(T&& value)
        {
            loggerPtr = std::forward<T>(value);
        }

        /**
         * Returns current logger instance.
         * @return Constant pointer to logger instance.
         */
        Loggers::ConstPointer_t getLogger() const
        {
            return loggerPtr.get();
        }


        /**
         * Sets auto-commit mode.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-autocommit.html
         *
         * @param value True for enabling auto-commit.
         * @throws MysqlInternalError When any error occurred during setting auto-commit flag.
         */
        void enableAutocommit(bool value)
        {
            if (mysql_autocommit(getMysqlPtr(), value))
            {
                throw MysqlInternalError("Failed to set auto-commit!");
            }
        }

        /**
         * Escapes string to be legal for using in SQL statements.
         * It using charset from MySQL's connection.
         * This can help prevent SQL-injection attacks.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-real-escape-string.html
         *
         * @tparam T Integer type of #originalLength param.
         * @param original C-style string.
         * @param originalLength String length.
         * @return SQL escaped string.
         */
        template<typename T>
        std::string escapeString(const char* original, T originalLength)
        {
            auto buffer = std::make_unique<char[]>(originalLength*2 + 1);
            auto length = mysql_real_escape_string(getMysqlPtr(), buffer.get(), original, originalLength);

            return std::string(buffer.get(), length);
        }

        /**
         * Escapes string to be legal for using in SQL statements.
         * It using charset from MySQL's connection.
         * This can help prevent SQL-injection attacks.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-real-escape-string.html
         *
         * @param original Original string.
         * @return SQL escaped string.
         */
        std::string escapeString(const std::string& original)
        {
            return escapeString(original.c_str(), original.length());
        }

        /**
         * Escapes string to be legal for using in SQL statements.
         * It doesn't have information about used charset!
         * This can help prevent SQL-injection attacks.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-escape-string.html
         *
         * @tparam T Integer type of #originalLength param.
         * @param original C-style string.
         * @param originalLength String length.
         * @return SQL escaped string.
         */
        template<typename T>
        static std::string escapeStringNoConnection(const char* original, T originalLength)
        {
            auto buffer = std::make_unique<char[]>(originalLength*2 + 1);
            auto length = mysql_escape_string(buffer.get(), original, originalLength);

            return std::string(buffer.get(), length);
        }

        /**
         * Escapes string to be legal for using in SQL statements.
         * It doesn't have information about used charset!
         * This can help prevent SQL-injection attacks.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-escape-string.html
         *
         * @param original Original string.
         * @return SQL escaped string.
         */
        static std::string escapeStringNoConnection(const std::string& original)
        {
            return escapeStringNoConnection(original.c_str(), original.length());
        }

        // XXX: makeHexString functions can be implemented by one function using constexpr-if from c++17
        /**
         * Converts string to HEX string suitable for using in SQL statements.
         * Useful for binary data for instance.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-hex-string.html
         *
         * @param from Original string.
         * @param fromlen Length of original string.
         * @return HEX string.
         */
        std::string makeHexString(const char *from, std::size_t fromlen)
        {
            std::string result(fromlen * 2 + 1, '\0');

            auto length = mysql_hex_string(&result.front(), from, fromlen);
            result.resize(length);

            return result;
        }

        /**
         * Converts string to HEX string suitable for using in SQL statements.
         * Useful for binary data for instance.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-hex-string.html
         *
         * @param from Original string.
         * @return HEX string.
         */
        std::string makeHexString(const char *from)
        {
            return makeHexString(from, std::strlen(from));
        }

        /**
         * Converts string to HEX string suitable for using in SQL statements.
         * Useful for binary data for instance.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-hex-string.html
         *
         * @param from Original string.
         * @return HEX string.
         */
        std::string makeHexString(const std::string &from)
        {
            return makeHexString(from.data(), from.size());
        }

        /**
         * Changes user and default DB for current connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-change-user.html
         *
         * @param user DB username.
         * @param password Password.
         * @param database Default database name.
         * @throws MysqlInternalError When any error occurred during changing the user on current connection.
         */
        void changeUser(const char* user, const char* password, const char* database)
        {
            using namespace std::string_literals;
            if (mysql_change_user(getMysqlPtr(), user, password, database))
            {
                throw MysqlInternalError("Failed to change user to '"s + user + "' on database '" + database + "'!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Returns the default charset name for the current connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-character-set-name.html
         *
         * @return Charset name as a C-string.
         */
        auto getCharacterSetName() noexcept
        {
            return mysql_character_set_name(getMysqlPtr());
        }

        /**
         * Describes client flag options of mysql_real_connect.
         * See https://dev.mysql.com/doc/c-api/8.0/en/mysql-real-connect.html
         */
        struct ClientFlags {
            unsigned long flags = 0;

            // Implicit default ctor can't be used as we're constructing this
            // in default argument of method of encapsulating class
            ClientFlags() : flags{} {}
            ClientFlags(unsigned long flags) : flags{flags} {}
        };

        /**
         * Connect (or reconnect) to MySQL server.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-real-connect.html
         *
         * @param host Hostname or IP address.
         * @param user DB username.
         * @param password Password.
         * @param database Default database name.
         * @param port Server port; Use 0 in case #sockedName is provided.
         * @param socketName Socket name; Use nullptr in case Hostname is provided.
         * @throws MysqlInternalError When any error occurred during connecting to server.
         */
        void connect(const char* host,
                     const char* user,
                     const char* password,
                     const char* database,
                     unsigned int port,
                     const char* socketName,
                     ClientFlags clientFlags = ClientFlags{})
        {
            using namespace std::string_literals;

            if (isConnected()) {
                mysqlClose();
                mysqlInit();
            }

            id = getGlobalIdRef().fetch_add(1);

            getLogger()->logMySqlConnecting(id, host, user, database, port, socketName);
            if (mysql_real_connect(getMysqlPtr(), host, user, password, database, port, socketName, clientFlags.flags | CLIENT_MULTI_STATEMENTS) == nullptr)
            {
                std::stringstream message{};
                message << "Failed to connect to MySQL. (Host: ";
                if (host != nullptr)
                {
                    message << host;
                }
                message << "; User: ";
                if (user != nullptr)
                {
                    message << user;
                }
                message << "; Database: ";
                if (database != nullptr)
                {
                    message << database;
                }
                message << "; Port: " << port;
                message << "; SocketName: ";
                if (socketName != nullptr)
                {
                    message << socketName;
                }

                throw MysqlInternalError(message.str(), mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }

            getLogger()->logMySqlConnected(id);
        }

        /**
         * Returns connection state.
         * @return True whether connection is active.
         */
        bool isConnected() const noexcept
        {
            return id != 0;
        }


        /**
         * Results management.
         */
        class Result
        {
        private:
            /** Pointer to underlying library's result set instance. */
            MYSQL_RES* resultPtr;

        public:
            /**
             * Constructs object directly from MySQL's result set represented by MYSQL_RES structure.
             * @param resultPtr Pointer to MYSQL_RES structure with result set.
             */
            explicit Result(MYSQL_RES* resultPtr)
                : resultPtr{resultPtr}
            {
                if (resultPtr == nullptr)
                {
                    throw InternalError("DBDriver result cannot be initialized with nullptr!");
                }
            }

            Result(const Result&) = delete;
            Result& operator=(const Result&) = delete;

            /**
             * Move constructor.
             * @param result Result instance.
             */
            Result(Result&& result) noexcept
            {
                resultPtr = result.resultPtr;
                result.resultPtr = nullptr;
            }

            /**
             * Move assignment operator.
             * @param result Result instance.
             * @return Reference to current instance.
             */
            Result& operator=(Result&& result) noexcept
            {
                static_assert(noexcept(freeResult()), "freeResult() must be noexcept!");
                freeResult();
                resultPtr = result.resultPtr;
                result.resultPtr = nullptr;
                return *this;
            }

            /**
             * Frees all allocated resources.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-free-result.html
             */
            ~Result()
            {
                freeResult();
            }


            /**
             * Seeks to an arbitrary row in a query result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-data-seek.html
             * @remark Use this method only in case #storeResult (not #useResult) was called before.
             *
             * @param index Row number in a result set.
             */
            void seekRow(size_t index) noexcept
            {
                mysql_data_seek(resultPtr, index);
            }

            /**
             * Sets the row cursor to an arbitrary row in a query result set by cursor offset.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-row-seek.html
             *
             * @param offset Row offset, not a row index. Typically returned value from #tellRowOffset is used.
             * @return The previous value of the row cursor
             */
            auto seekRowOffset(MYSQL_ROW_OFFSET offset) noexcept
            {
                return mysql_row_seek(resultPtr, offset);
            }

            /**
             * Returns the current position of the row cursor for the last #fetchRow call.
             *
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-row-tell.html
             * @remark Use this method only in case #storeResult (not #useResult) was called before.
             *
             * @return The current offset of the row cursor.
             */
            auto tellRowOffset() noexcept
            {
                return mysql_row_tell(resultPtr);
            }

            /**
             * Returns the definition of one column of a result set.
             * Call this function repeatedly to retrieve information about all columns in the result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-field.html
             *
             * @return The MYSQL_FIELD structure for the current column. NULL if no columns are left.
             */
            auto fetchField() noexcept
            {
                return mysql_fetch_field(resultPtr);
            }

            /**
             * Returns column's field definition for the specified column in a result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-field-direct.html
             *
             * @param index Column index to fetch details of.
             * @return The MYSQL_FIELD structure for the specified column.
             */
            auto fetchFieldDirect(unsigned int index) noexcept
            {
                return mysql_fetch_field_direct(resultPtr, index);
            }

            /**
             * Returns an array of all MYSQL_FIELD structures for a result set.
             * Each structure provides the field definition for one column of the result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-fields.html
             *
             * @return An array of MYSQL_FIELD structures for all columns of a result set.
             */
            auto fetchFields() noexcept
            {
                return mysql_fetch_fields(resultPtr);
            }

            /**
             * Returns the lengths of the columns of the current row within a result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-lengths.html
             *
             * @return An array of unsigned long integers representing the size of each column (not including any terminating null bytes).
             */
            auto fetchLengths() noexcept
            {
                return mysql_fetch_lengths(resultPtr);
            }

            /**
             * Retrieves the next row of a result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-row.html
             *
             * @return MYSQL_ROW with next row results. NULL if there are no more rows or on error.
             */
            auto fetchRow() noexcept
            {
                return mysql_fetch_row(resultPtr);
            }

            /**
             * Retrieves the next row of a result set. Method checks for error and throws appropriate exception.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-fetch-row.html
             *
             * @return MYSQL_ROW with next row results. NULL if there are no more rows.
             */
            auto checkedFetchRow()
            {
                auto *row = mysql_fetch_row(resultPtr);

                if (row == nullptr && mysql_errno(resultPtr->handle) != 0)
                {
                    throw RuntimeError { std::string("Failed to fetch row: '") + mysql_error(resultPtr->handle) + "'" };
                }

                return row;
            }

            /**
             * Sets the field cursor to the given offset / index.
             * The next call to #fetchField retrieves the field definition of the column associated with that offset.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-field-seek.html
             *
             * @param offset Field number index, use 0 for first column.
             * @return Previous offset value.
             */
            auto fieldSeek(MYSQL_FIELD_OFFSET offset) noexcept
            {
                return mysql_field_seek(resultPtr, offset);
            }

            /**
             * Returns the position of the field cursor used for the last #fetchField call.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-field-tell.html
             *
             * @return The current offset of the field cursor.
             */
            auto fieldTell() noexcept
            {
                return mysql_field_tell(resultPtr);
            }

            /**
             * Frees allocated memory for current result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-free-result.html
             */
            void freeResult() noexcept
            {
                if (resultPtr != nullptr)
                {
                    mysql_free_result(resultPtr);
                    resultPtr = nullptr;
                }
            }

            /**
             * Returns number of columns in a result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-num-fields.html
             *
             * @return Unsigned integer representing the number of columns in a result set.
             */
            auto getFieldsCount() noexcept
            {
                return mysql_num_fields(resultPtr);
            }

            /**
             * Returns number of records in a result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-num-rows.html
             *
             * @return Number of rows in a result set.
             */
            auto getRowsCount() noexcept
            {
                return mysql_num_rows(resultPtr);
            }


            /**
             * Returns low-level result set instance.
             * @remark DO NOT USE this function unless you want to work with C API directly!
             *
             * @return Pointer to MYSQL_RES structure with current result set.
             */
            auto detail_getResultPtr() noexcept
            {
                return resultPtr;
            }
        };


        /**
         * Returns the number of columns for the most recent query on the connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-field-count.html
         *
         * @return Number of columns of the last performed query.
         */
        auto getFieldsCount() noexcept
        {
            return mysql_field_count(getMysqlPtr());
        }

        /**
         * Returns MySQL client library version string.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-client-info.html
         *
         * @return MySQL client library version as a string.
         */
        static std::string getClientInfo()
        {
            return mysql_get_client_info();
        }

        /**
         * Returns MySQL client library version as an integer value.
         * For instance, "5.7.23" is represented as 50723.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-client-version.html
         *
         * @return MySQL client library version as an integer value.
         */
        static auto getClientVersion() noexcept
        {
            return mysql_get_client_version();
        }

        /**
         * Returns a string describing the connection type in use.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-host-info.html
         *
         * @return String representing server hostname and connection type.
         */
        std::string getHostInfo()
        {
            return mysql_get_host_info(getMysqlPtr());
        }

        /**
         * Provides info about character set on the client.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-character-set-info.html
         *
         * @return Instance of MY_CHARSET_INFO structure.
         */
        auto getCharacterSetInfo() noexcept
        {
            MY_CHARSET_INFO characterSet;
            mysql_get_character_set_info(getMysqlPtr(), &characterSet);
            return characterSet;
        }

        /**
         * Returns protocol version used by current connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-proto-info.html
         *
         * @return Integer representing protocol version of current connection.
         */
        auto getProtocolInfo() noexcept
        {
            return mysql_get_proto_info(getMysqlPtr());
        }

        /**
         * Returns MySQL server version string.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-server-info.html
         *
         * @return MySQL server version as a string.
         */
        std::string getServerInfo()
        {
            return mysql_get_server_info(getMysqlPtr());
        }

        /**
         * Returns MySQL server library version as an integer value.
         * For instance, "5.7.23" is represented as 50723.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-server-version.html
         *
         * @return MySQL server version as an integer value.
         */
        auto getServerVersion() noexcept
        {
            return mysql_get_server_version(getMysqlPtr());
        }

        /**
         * Returns encryption cipher used for current connection to the server.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-get-ssl-cipher.html
         *
         * @return String naming the encryption cipher user for connection.
         *         Empty string is returned in case the connection is not encrypted.
         */
        std::string getSslCipher()
        {
            auto stringPtr = mysql_get_ssl_cipher(getMysqlPtr());
            if (stringPtr == nullptr)
            {
                return {};
            }
            else
            {
                return stringPtr;
            }
        }

        /**
         * Last value generated for AUTO_INCREMENT column for the last performed
         * INSERT or UPDATE statement.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-insert-id.html
         *
         * @return Last ID value for AUTO_INCREMENT column.
         */
        auto getInsertId() noexcept
        {
            return mysql_insert_id(getMysqlPtr());
        }

        /**
         * Executes SQL statement described in queryString.
         * Call #storeResult or #useResult to get the #Result instance.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-real-query.html
         *
         * @param queryString SQL query as a C-string.
         * @param length SQL query length.
         * @throws MysqlInternalError When any error occurred.
         */
        void execute(const char* queryString, unsigned long length)
        {
            getLogger()->logMySqlQuery(id, StringView{queryString, length});
            if (mysql_real_query(getMysqlPtr(), queryString, length))
            {
                throw MysqlInternalError("Failed to execute query!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Executes SQL statement described in queryString.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-real-query.html
         *
         * @param queryString SQL query to execute.
         * @throws MysqlInternalError When any error occurred.
         */
        void execute(const std::string& queryString)
        {
            execute(queryString.c_str(), queryString.length());
        }

        /**
         * May be called immediately after executing statement by #execute
         * method to get number of affected rows by last SELECT, UPDATE, INSERT
         * or DELETE statement.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-affected-rows.html
         *
         * @return Number of affected rows by last statement.
         */
        auto affectedRows() noexcept
        {
            return mysql_affected_rows(getMysqlPtr());
        }

        /**
         * Detects whether any result is available to fetch.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-more-results.html
         *
         * @return True if there are more results.
         */
        bool hasMoreResults() noexcept
        {
            return mysql_more_results(getMysqlPtr());
        }

        /**
         * Fetch next result and returns state to idicate whether more results exists.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-next-result.html
         *
         * @return True if there are more results.
         * @throws MysqlInternalError When any error occurred.
         */
        bool nextResult()
        {
            auto result = mysql_next_result(getMysqlPtr());
            switch (result)
            {
                case 0:
                    return true;
                case -1:
                    return false;
                default:
                    throw MysqlInternalError("Failed to get next result!",
                        mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Enumeration of MySQL options, used instead of #mysql_option.
         */
        enum class DriverOptions : std::underlying_type_t<mysql_option>
        {
              connectTimeout = MYSQL_OPT_CONNECT_TIMEOUT,
              compress = MYSQL_OPT_COMPRESS,
              namedPipe = MYSQL_OPT_NAMED_PIPE,
              initCommand = MYSQL_INIT_COMMAND,
              readDefaultFile = MYSQL_READ_DEFAULT_FILE,
              readDefaultGroup = MYSQL_READ_DEFAULT_GROUP,
              setCharsetDir = MYSQL_SET_CHARSET_DIR,
              setCharsetName = MYSQL_SET_CHARSET_NAME,
              localInfile = MYSQL_OPT_LOCAL_INFILE,
              protocol = MYSQL_OPT_PROTOCOL,
              sharedMemoryBaseName = MYSQL_SHARED_MEMORY_BASE_NAME,
              readTimeout = MYSQL_OPT_READ_TIMEOUT,
              writeTimeout = MYSQL_OPT_WRITE_TIMEOUT,
              useResult = MYSQL_OPT_USE_RESULT,
              useRemoteConnection = MYSQL_OPT_USE_REMOTE_CONNECTION,
              useEmbeddedConnection = MYSQL_OPT_USE_EMBEDDED_CONNECTION,
              guessConnection = MYSQL_OPT_GUESS_CONNECTION,
              setClientIp = MYSQL_SET_CLIENT_IP,
              secureAuthentication = MYSQL_SECURE_AUTH,
              reportDataTruncation = MYSQL_REPORT_DATA_TRUNCATION,
              reconnect = MYSQL_OPT_RECONNECT,
              sslVerifyServerCertificate = MYSQL_OPT_SSL_VERIFY_SERVER_CERT,
              pluginDir = MYSQL_PLUGIN_DIR,
              defaultAuthentication = MYSQL_DEFAULT_AUTH,
              enableClearTextPlugin = MYSQL_ENABLE_CLEARTEXT_PLUGIN,
        };

        /**
         * Sets an extra connect options and affects behavior for a connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-options.html
         *
         * @param option Option to set on client/library side.
         * @param argumentPtr New value for option.
         * @throws MysqlInternalError When any error occurred during setting client options.
         */
        void setDriverOption(DriverOptions option, const void* argumentPtr)
        {
            if (mysql_options(getMysqlPtr(), static_cast<mysql_option>(option), argumentPtr))
            {
                throw MysqlInternalError("Failed to set option!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Checks whether the connection to the server is working.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-ping.html
         *
         * @throws MysqlInternalError in case the connection is not alive.
         */
        void ping()
        {
            getLogger()->logMySqlPing(id);
            if (mysql_ping(getMysqlPtr()))
            {
                throw MysqlInternalError("Failed to ping server!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Checks whether the connection to the server is working.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-ping.html
         *
         * @return True if a connection is still alive, false is returned otherwise.
         */
        bool tryPing()
        {
            getLogger()->logMySqlPing(id);
            return !mysql_ping(getMysqlPtr());
        }

        /**
         * Retrieves additional human-readable information about the most recently executed statement.
         * The statement must be one of the following types: INSERT, LOAD DATA INFILE, ALTER TABLE, UPDATE.
         * Empty string is returned for all other statements then the types listed above.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-info.html
         *
         * @return String representing additional information about the last performed SQL statement.
         */
        std::string queryInfo()
        {
            const char* info = mysql_info(getMysqlPtr());
            if (info == nullptr)
            {
                return {};
            }
            else
            {
                return info;
            }
        }

        /**
         * Sets the default character set for the current connection.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-set-character-set.html
         *
         * @param characterSetName Character set name.
         * @throws MysqlInternalError When any error occurred during changing the charset.
         */
        void setCharacterSet(char* characterSetName)
        {
            if (mysql_set_character_set(getMysqlPtr(), characterSetName))
            {
                throw MysqlInternalError("Failed to set character set!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Enumeration of MySQL server-side options, used instead of #enum_mysql_set_option.
         */
        enum class ServerOptions : std::underlying_type_t<enum_mysql_set_option>
        {
              multiStatementsOn = MYSQL_OPTION_MULTI_STATEMENTS_ON,
              multiStatementsOff = MYSQL_OPTION_MULTI_STATEMENTS_OFF,
        };

        /**
         * Enables or disables the multiple statement option for a connection on server side.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-set-server-option.html
         *
         * @param option Option to set on server side.
         * @throws MysqlInternalError When any error occurred during setting server options.
         */
        void setServerOption(ServerOptions option)
        {
            if (mysql_set_server_option(getMysqlPtr(), static_cast<enum_mysql_set_option>(option)))
            {
                throw MysqlInternalError("Failed to set server option!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
        }

        /**
         * Establish encrypted connection using SSL.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-ssl-set.html
         *
         * @param key The path name of the client private key file.
         * @param certificate The path name of the client public key certificate file.
         * @param certificationAuthority The path name of the Certificate Authority (CA) certificate file.
         * @param trustedCertificationAuthorityDirectory The path name of the directory that contains trusted SSL CA certificate files.
         * @param cipher List The list of permitted ciphers for SSL encryption.
         */
        void setSsl(
                const char* key,
                const char* certificate,
                const char* certificationAuthority,
                const char* trustedCertificationAuthorityDirectory,
                const char* cipherList) noexcept
        {
            mysql_ssl_set(getMysqlPtr(), key, certificate, certificationAuthority,
                    trustedCertificationAuthorityDirectory, cipherList);
        }

        /**
         * Return MySQL server statistics string.
         * Statistics data contains uptime [s], number of running threads, questions, reloads and open tables.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stat.html
         *
         * @return MySQL server statistics string in readable format.
         * @throws MysqlInternalError When any error occurred.
         */
        std::string serverStatistics()
        {
            auto statisticsPtr = mysql_stat(getMysqlPtr());
            if (statisticsPtr == nullptr)
            {
                throw MysqlInternalError("Failed to get server statistics!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
            return statisticsPtr;
        }

        /**
         * Returns the number of errors, warning and notes for last performed SQL statement.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-warning-count.html
         *
         * @return Number of errors, warning and notes during the last SQL statement.
         */
        auto warningsCount() noexcept
        {
            return mysql_warning_count(getMysqlPtr());
        }


        /**
         * Prepared statements.
         */
        class Statement
        {
        private:
            /** Pointer to underlying library's statement handler instance. */
            MYSQL_STMT* statementPtr;
            /** ID of parent entity - DBDriver instance; Used only for logging purposes. */
            std::uint_fast64_t driverId;
            /** Internal ID; each instance have unique ID. */
            std::uint_fast64_t id;
            /** Logger instance pointer. */
            Loggers::ConstPointer_t loggerPtr;

            /**
             * Internal thread-safe ID counter.
             * @return ID counter reference.
             */
            static auto& getGlobalIdRef() noexcept
            {
                static std::atomic<std::uint_fast64_t> globalId{1};
                return globalId;
            }

            /**
             * Closes the prepared statement and deallocates the statement handler.
             * @throws MysqlInternalError If failed to close the statement.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-close.html
             */
            void close()
            {
                if (statementPtr != nullptr)
                {
                    loggerPtr->logMySqlStmtClose(driverId, id);
                    MYSQL* mysql = statementPtr->mysql;
                    if (mysql_stmt_close(statementPtr))
                    {
                        throw MysqlInternalError("Failed to close statement!",
                            mysql_error(mysql), mysql_errno(mysql));
                    }
                }
            }

        public:
            Statement() = delete;

            /**
             * Constructs prepared statement on passed connection.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-init.html
             *
             * @param mysql Connection handler settings.
             * @param driverId DBDriver ID for logging purposes.
             * @param loggerPtr Pointer to Logger object.
             */
            Statement(MYSQL& mysql, std::uint_fast64_t driverId, Loggers::ConstPointer_t loggerPtr)
                : driverId{driverId}, id{getGlobalIdRef().fetch_add(1)}, loggerPtr{std::move(loggerPtr)}
            {
                statementPtr = mysql_stmt_init(&mysql);
                if (statementPtr == nullptr)
                {
                    throw MysqlInternalError("Could not initialize statement!",
                        mysql_error(&mysql), mysql_errno(&mysql));
                }

            }

            Statement(const Statement&) = delete;
            Statement& operator=(const Statement&) = delete;

            /**
             * Move constructor.
             * @param result Statement instance.
             */
            Statement(Statement&& other) noexcept
                : statementPtr{std::move(other).statementPtr},
                  driverId{std::move(other).driverId},
                  id{std::move(other).id},
                  loggerPtr{std::move(other).loggerPtr}
            {
                other.statementPtr = nullptr;
            }

            /**
             * Move assignment operator.
             * @param result Statement instance.
             * @return Reference to current instance.
             * @throws MysqlInternalError If failed to close the statement.
             */
            Statement& operator=(Statement&& other)
            {
                close();
                statementPtr = other.statementPtr;
                other.statementPtr = nullptr;
                return *this;
            }

            /**
             * Closes the prepared statement and frees all allocated resources.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-close.html
             */
            ~Statement()
            {
                try
                {
                    close();
                }
                catch (const MysqlInternalError& e)
                {
                    loggerPtr->logMySqlStmtCloseError(driverId, id, e.getMysqlError());
                }
            }

            /**
             * Returns parent DBDriver ID.
             * @return DBDriver ID.
             */
            auto getDriverId() const noexcept
            {
                return driverId;
            }

            /**
             * Returns unique statement's ID.
             * @return Statement's ID.
             */
            auto getId() const noexcept
            {
                return id;
            }

            void prepare(const char* query, unsigned long length)
            {
                loggerPtr->logMySqlStmtPrepare(driverId, id, StringView{query, length});
                if (mysql_stmt_prepare(statementPtr, query, length))
                {
                    throw MysqlInternalError("Failed to prepare statement!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            void prepare(const std::string& query)
            {
                prepare(query.c_str(), query.length());
            }

            void bindParam(MYSQL_BIND* bindingsArrayPtr)
            {
                if (mysql_stmt_bind_param(statementPtr, bindingsArrayPtr))
                {
                    throw MysqlInternalError("Failed to bind statement's params!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            void execute()
            {
                loggerPtr->logMySqlStmtExecute(driverId, id);
                if (mysql_stmt_execute(statementPtr))
                {
                    throw MysqlInternalError("Failed to execute statement!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            auto freeResult() noexcept
            {
                return mysql_stmt_free_result(statementPtr);
            }

            auto fieldCount() noexcept
            {
                return mysql_stmt_field_count(statementPtr);
            }

            auto insertedId() noexcept
            {
                return mysql_stmt_insert_id(statementPtr);
            }

            auto resultMetadata()
            {
                auto resultPtr = mysql_stmt_result_metadata(statementPtr);
                if (resultPtr == nullptr)
                {
                    throw MysqlInternalError("Could not get result statement metadata!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
                return Result{resultPtr};
            }

            void bindResult(MYSQL_BIND* bindingsArrayPtr)
            {
                if (mysql_stmt_bind_result(statementPtr, bindingsArrayPtr))
                {
                    throw MysqlInternalError("Failed to bind statement's result!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            void storeResult()
            {
                if (mysql_stmt_store_result(statementPtr))
                {
                    throw MysqlInternalError("Failed to store statement's result!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            enum class FetchStatus
            {
                Ok,
                NoMoreData,
                DataTruncated,
            };

            FetchStatus fetchWithStatus()
            {
                auto result = mysql_stmt_fetch(statementPtr);
                switch (result)
                {
                    case 0:
                        return FetchStatus::Ok;

                    case MYSQL_NO_DATA:
                        return FetchStatus::NoMoreData;

                    case 1:
                        throw MysqlInternalError{"Could not fetch statement!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr)};

                    case MYSQL_DATA_TRUNCATED:
                        return FetchStatus::DataTruncated;

                    default:
                        throw LogicError{"Internal error!"};
                }
            }

            bool fetch()
            {
                auto result = fetchWithStatus();
                switch (result)
                {
                    case FetchStatus::Ok:
                        return true;

                    case FetchStatus::NoMoreData:
                        return false;

                    case FetchStatus::DataTruncated:
                        throw MysqlDataTruncatedError{"Data truncated while fetching statement!" + [&](){
                            // Identify truncated columns
                            auto&& resultBindingsPtr = statementPtr->bind;
                            auto resultBindingsSize = statementPtr->field_count;
                            assert(resultBindingsSize > 0u);

                            std::vector<std::size_t> truncatedColumns{};
                            std::vector<std::size_t> undetectedColumns{};
                            std::size_t index = 0;
                            for (auto it=resultBindingsPtr; it!=resultBindingsPtr+resultBindingsSize; ++it)
                            {
                                if (it->error == &it->error_value)
                                {
                                    if (it->error_value)
                                    {
                                        truncatedColumns.emplace_back(index);
                                    }
                                }
                                else
                                {
                                    undetectedColumns.emplace_back(index);
                                }
                                ++index;
                            }

                            std::string result{" Truncated columns: "};
                            if (truncatedColumns.size() == 0)
                            {
                                result += "None.";
                            }
                            else
                            {
                                result += "[";
                                result += toString(truncatedColumns);
                                result += "].";
                            }

                            if (undetectedColumns.size() > 0)
                            {
                                result += " Following columns truncation state could not have been detected since you have set custom error pointer: ";
                                result += toString(undetectedColumns);
                                result += "!!!";
                            }

                            return result;
                        }()};

                    default:
                        throw LogicError{"Internal error!"};
                }
            }

            void fetchColumn(MYSQL_BIND* bindings, unsigned int column, unsigned long offset)
            {
                if (mysql_stmt_fetch_column(statementPtr, bindings, column, offset))
                {
                    throw MysqlInternalError("Failed to fetch statement's column! Invalid column number!");
                }
            }

            /**
             * May be called immediately after executing statement by #execute
             * method to get number of affected rows by last SELECT, UPDATE, INSERT
             * or DELETE statement.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-affected-rows.html
             *
             * @return Number of affected rows by last statement.
             */
            auto affectedRows() noexcept
            {
                return mysql_stmt_affected_rows(statementPtr);
            }

            /**
             * Returns the number of parameters markers present in prepared statement.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-param-count.html
             *
             * @return Number of parameters in a statement.
             */
            auto paramCount() noexcept
            {
                return mysql_stmt_param_count(statementPtr);
            }

            void sendLongData(unsigned int paramNumber, const char* data, unsigned long length)
            {
                if (mysql_stmt_send_long_data(statementPtr, paramNumber, data, length))
                {
                    throw MysqlInternalError("Failed to store statement's result!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            auto sendLongData(unsigned int paramNumber, const std::string& data)
            {
                return sendLongData(paramNumber, data.c_str(), data.length());
            }

            std::string sqlState()
            {
                return mysql_stmt_sqlstate(statementPtr);
            }

            /**
             * Seeks to an arbitrary row in a statement result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-data-seek.html
             * @remark Use this method only in case #storeResult (not #useResult) was called before.
             *
             * @param index Row number in a result set.
             */
            void seekRow(size_t index) noexcept
            {
                mysql_stmt_data_seek(statementPtr, index);
            }

            /**
             * Sets the row cursor to an arbitrary row in a statement result set by cursor offset.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-row-seek.html
             *
             * @param offset Row offset, not a row index. Typically returned value from #tellRowOffset is used.
             * @return The previous value of row cursor.
             */
            auto seekRowOffset(MYSQL_ROW_OFFSET offset) noexcept
            {
                return mysql_stmt_row_seek(statementPtr, offset);
            }

            /**
             * Returns the current position of the row cursor for the last #fetchRow call.
             *
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-row-tell.html
             * @remark Use this method only in case #storeResult (not #useResult) was called before.
             *
             * @return The current offset of the row cursor.
             */
            auto tellRowOffset() noexcept
            {
                return mysql_stmt_row_tell(statementPtr);
            }

            /**
             * Returns the number of rows in the statement's result set.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-num-rows.html
             * @remark Call this method for SELECT queries after the #storeResult method call.
             *
             * @return Number of rows in the statement's result set.
             */
            auto getRowsCount() noexcept
            {
                return mysql_stmt_num_rows(statementPtr);
            }

            enum class Attributes : std::underlying_type_t<enum_stmt_attr_type>
            {
                updateMaxLength = STMT_ATTR_UPDATE_MAX_LENGTH,
                cursorType = STMT_ATTR_CURSOR_TYPE,
                prefetchRows = STMT_ATTR_PREFETCH_ROWS,
            };

            template<typename T>
            void setAttribute(Attributes attribute, const T& argument)
            {
                if (mysql_stmt_attr_set(statementPtr, static_cast<enum_stmt_attr_type>(attribute), &argument))
                {
                    throw MysqlInternalError("Failed to set statement attribute!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            template<typename T>
            void getAttribute(Attributes attribute, T& argument)
            {
                if (mysql_stmt_attr_get(statementPtr, static_cast<enum_stmt_attr_type>(attribute), &argument))
                {
                    throw MysqlInternalError("Failed to get statement attribute!",
                        mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            /**
             * Detects whether any result is available to fetch.
             * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-stmt-next-result.html
             *
             * @return True if there are more results.
             * @throws MysqlInternalError When any error occurred.
             */
            bool nextResult()
            {
                auto result = mysql_stmt_next_result(statementPtr);
                switch (result)
                {
                    case 0:
                        return true;
                    case -1:
                        return false;
                    default:
                        throw MysqlInternalError("Failed to get next statement's result!",
                            mysql_stmt_error(statementPtr), mysql_stmt_errno(statementPtr));
                }
            }

            /**
             * Returns low-level prepared statement instance.
             * @remark DO NOT USE this function unless you want to work with C API directly!
             *
             * @return Pointer to MYSQL_STMT structure with prepared statement.
             */
            auto detail_getStatementPtr() noexcept
            {
                return statementPtr;
            }
        };

        /**
         * Factory method for constructing #Statement object providing interface for prepared statements.
         * @return Prepared statements management instance.
         */
        Statement makeStatement()
        {
            return {mysql, id, loggerPtr.get()};
        }

        /**
         * Retrieve the entire result set of last executed query to the client.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-store-result.html
         *
         * @return Results management instance.
         * @throws MysqlInternalError When any error occurred.
         */
        auto storeResult()
        {
            auto result = mysql_store_result(getMysqlPtr());
            if (result == nullptr)
            {
                auto fieldsCount = getFieldsCount();
                if (fieldsCount == 0)
                {
                    // this query should have returned no result (it's INSERT, UPDATE, ...)
                    throw LogicError("You cann't store result for query that shall have no result!!");
                }
                else
                {
                    throw MysqlInternalError("Failed to store result!",
                        mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
                }
            }
            return Result{result};
        }

        /**
         * Retrieve each result's row individually from server without storing it locally.
         * @see https://dev.mysql.com/doc/refman/5.7/en/mysql-use-result.html
         *
         * @return Results management instance.
         * @throws MysqlInternalError When any error occurred.
         */
        auto useResult()
        {
            auto result = mysql_use_result(getMysqlPtr());
            if (result == nullptr)
            {
                throw MysqlInternalError("Failed to use result!",
                    mysql_error(getMysqlPtr()), mysql_errno(getMysqlPtr()));
            }
            return Result{result};
        }

        /**
         * Returns unique instance's ID.
         * @return Instance's ID.
         */
        auto getId() const noexcept
        {
            return id;
        }


        /**
         * Returns low-level configuration used for current connection.
         * @remark DO NOT USE this function unless you want to work with C API directly!
         *
         * @return Pointer to MYSQL structure with current connection settings.
         */
        MYSQL* detail_getMysqlPtr() noexcept
        {
            return getMysqlPtr();
        }
    };


    /**
     * Identity comparison of two DBDriver instances by comparing its unique IDs.
     * @param lhs DBDriver instance.
     * @param rhs DBDriver instance.
     * @return True if both DBDriver instances are the same.
     */
    inline bool operator==(const DBDriver& lhs, const DBDriver& rhs) noexcept
    {
        return lhs.getId() == rhs.getId();
    }
}}
