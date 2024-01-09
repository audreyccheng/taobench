/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <string>
#include <stdexcept>
#include <superior_mysqlpp/types/string_view.hpp>

namespace SuperiorMySqlpp
{
    class SuperiorMySqlppError : public std::runtime_error
    {
    public:
        explicit SuperiorMySqlppError(const std::string& message)
            : std::runtime_error{message}
        {
        }
    };


    class MysqlInternalError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;

        MysqlInternalError(const std::string& message,
                           const char* mysqlError_,
                           unsigned errorCode_)
            : SuperiorMySqlppError(message + "\nMysql error: " + mysqlError_)
            , mysqlError{mysqlError_}
            , errorCode{errorCode_}
        {
        }

        StringView getMysqlError() const
        {
            return mysqlError;
        }

        unsigned getErrorCode() const
        {
            return errorCode;
        }

    private:
        std::string mysqlError;
        const unsigned errorCode {0};
    };

    class MysqlDataTruncatedError: public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class RuntimeError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class LogicError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class OutOfRange : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class InternalError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class PreparedStatementTypeError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class PreparedStatementBindError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class DynamicPreparedStatementTypeError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class DynamicPreparedStatementLogicError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class QueryError : public SuperiorMySqlppError
    {
    public:
        using SuperiorMySqlppError::SuperiorMySqlppError;
    };

    class QueryNotExecuted : public QueryError
    {
    public:
        using QueryError::QueryError;
    };

    /**
     * @brief Error for unexpected row count. Number of rows accessible via `getRowCount()` method
     */
    class UnexpectedRowCountError : public SuperiorMySqlppError
    {
    public:
        UnexpectedRowCountError(const std::string& message,
                                size_t rowCount_)
            : SuperiorMySqlppError { message }
            , rowCount(rowCount_)
        {
        }

        inline size_t getRowCount() const
        {
            return rowCount;
        }

    private:
        size_t rowCount;
    };

}
