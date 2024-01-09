/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <cassert>

#include <superior_mysqlpp/connection_def.hpp>
#include <superior_mysqlpp/query.hpp>
#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/uncaught_exception_counter.hpp>


namespace SuperiorMySqlpp
{
    enum class IsolationLevel
    {
        ReadUncommitted,
        ReadCommitted,
        RepeatableRead,
        Serializable
    };

    enum class TransactionCharacteristics
    {
        WithConsistentSnapshot,
        ReadOnly,
        ReadWrite
    };


    namespace detail
    {
        inline const char* toQueryString(IsolationLevel isolationLevel)
        {
            switch (isolationLevel)
            {
                case IsolationLevel::ReadUncommitted:
                    return "READ UNCOMMITTED";

                case IsolationLevel::ReadCommitted:
                    return "READ COMMITTED";

                case IsolationLevel::RepeatableRead:
                    return "REPEATABLE READ";

                case IsolationLevel::Serializable:
                    return "SERIALIZABLE";
            }
            assert(false);
        }

        inline const char* toQueryString(TransactionCharacteristics transactionCharacteristics)
        {
            switch (transactionCharacteristics)
            {
                case TransactionCharacteristics::WithConsistentSnapshot:
                    return "WITH CONSISTENT SNAPSHOT";

                case TransactionCharacteristics::ReadOnly:
                    return "READ ONLY";

                case TransactionCharacteristics::ReadWrite:
                    return "READ WRITE";
            }
            assert(false);
        }
    }

    class Transaction
    {
    private:
        Connection& connection;
        UncaughtExceptionCounter uncaughtExceptionCounter{};

    public:
        Transaction(Connection& connection)
            : connection{connection}
        {
            connection.getLogger()->logMySqlStartTransaction(connection.getId());

            connection.makeQuery("START TRANSACTION").execute();
        }

        Transaction(Connection& connection,
                    TransactionCharacteristics transactionCharacteristics)
            : connection{connection}
        {
            auto query = connection.makeQuery("START TRANSACTION ");
            query << detail::toQueryString(transactionCharacteristics);

            connection.getLogger()->logMySqlStartTransaction(connection.getId());

            query.execute();
        }

        Transaction(Connection& connection,
                    IsolationLevel isolationLevel)
            : connection{connection}
        {
            auto query = connection.makeQuery("SET TRANSACTION ISOLATION LEVEL ");
            query << detail::toQueryString(isolationLevel);
            query << "; START TRANSACTION;";

            connection.getLogger()->logMySqlStartTransaction(connection.getId());

            query.execute();
            while (query.nextResult()) {}
        }

        Transaction(Connection& connection,
                    IsolationLevel isolationLevel,
                    TransactionCharacteristics transactionCharacteristics)
            : connection{connection}
        {
            auto query = connection.makeQuery("SET TRANSACTION ISOLATION LEVEL ");
            query << detail::toQueryString(isolationLevel);
            query << "; START TRANSACTION " << detail::toQueryString(transactionCharacteristics) << ";";

            connection.getLogger()->logMySqlStartTransaction(connection.getId());

            query.execute();
            while (query.nextResult()) {}
        }

        Transaction(const Transaction&) = default;
        Transaction(Transaction&&) = default;
        Transaction& operator=(const Transaction&) = default;
        Transaction& operator=(Transaction&&) = default;

        ~Transaction() noexcept(false)
        {
            if (uncaughtExceptionCounter.isNewUncaughtException())
            {
                /*
                 * Prevent double-exception situation!!!
                 */
                try
                {
                    connection.getLogger()->logMySqlRollbackTransaction(connection.getId());
                    connection.makeQuery("ROLLBACK").execute();
                }
                catch (std::exception& exception)
                {
                    connection.getLogger()->logMySqlTransactionRollbackFailed(connection.getId(), exception);
                }
                catch (...)
                {
                    connection.getLogger()->logMySqlTransactionRollbackFailed(connection.getId());
                }
            }
            else
            {
                connection.getLogger()->logMySqlCommitTransaction(connection.getId());
                connection.makeQuery("COMMIT").execute();
            }
        }
    };
}

