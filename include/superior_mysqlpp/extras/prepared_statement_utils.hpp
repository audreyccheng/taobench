#pragma once

#include <superior_mysqlpp/connection.hpp>
#include <superior_mysqlpp/exceptions.hpp>
#include <superior_mysqlpp/prepared_statement.hpp>
#include <superior_mysqlpp/utils.hpp>
#include <type_traits>

namespace SuperiorMySqlpp
{
    namespace detail
    {
        /**
         * Converts tuples of arguments into prepared statement via `generate()` method
         * @tparam ResultArgs tuple of arguments from result function
         * @tparam ParamsArgs tuple of arguments from params function
         */
        template<typename ParamsArgs, typename ResultArgs>
        struct ToPreparedStatement;

        template<template<typename...> class ParamsTuple, typename... ParamsArgs, template<typename...> class ResultTuple, typename... ResultArgs>
        struct ToPreparedStatement<ParamsTuple<ParamsArgs...>, ResultTuple<ResultArgs...>>
        {
            /**
             * Constructs prepared statement
             * @param connection Connection handle into database
             * @param query Query for prepared statement
             * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
             * @tparam validateMode Indicates validate mode level
             * @tparam warnMode Indicates warning mode level
             * @tparam ignoreNullable Disables null type checking
             */
            template <bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable>
            static inline auto generate(Connection &connection, const std::string &query)
            {
                return PreparedStatement<ResultBindings<ResultArgs...>, ParamBindings<ParamsArgs...>, storeResult, validateMode, warnMode, ignoreNullable>
                    { connection, query };
            }
        };

        /**
         * @brief Makes prepared statement by deducing ResultBindings and ParamBindings template arguments from functions's signature
         * @param connection Connection handle into database
         * @param query Query for prepared statement
         * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
         * @tparam validateMode Indicates validate mode level
         * @tparam warnMode Indicates warning mode level
         * @tparam ignoreNullable Disables null type checking
         */
        template <bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable, typename ResultCallable, typename ParamCallable>
        auto generatePreparedStatement(Connection &connection, const std::string &query, ParamCallable &&, ResultCallable &&)
        {
            static_assert(FunctionInfo<ParamCallable>::template transform_args<AreArgumentsLvalueRefs>::value, "Arguments must be lvalue references");
            return ToPreparedStatement<
                typename FunctionInfo<ParamCallable>::template transform_args<RemoveCVRefArgs>::type,
                typename FunctionInfo<ResultCallable>::template transform_args<RemoveCVRefArgs>::type
            >::template generate<storeResult, validateMode, warnMode, ignoreNullable>(connection, query);
        }

        /**
         * @brief Reads one and only one row from your own prepared statement into variables
         * @param ps Prepared statement object (only static statements are currently supported)
         * @param values References to variables to be loaded from query
         *               Their type must be compatible with query result types
         * @throws UnexpectedRowCountError if number of results is not exactly 1
         */
        template<typename PreparedStatementType, typename... Args, typename = typename std::enable_if<std::is_base_of<detail::StatementBase, std::remove_reference_t<PreparedStatementType>>::value>::type>
        void psReadValuesImpl(PreparedStatementType &&ps, Args&... values)
        {
            ps.execute();

            if (ps.getRowsCount() != 1 || !ps.fetch())
            {
                throw UnexpectedRowCountError("psReadValues() expected exactly one row, got " + std::to_string(ps.getRowsCount()) + " rows", ps.getRowsCount());
            }

            std::forward_as_tuple(values...) = ps.getResult();
        }
    }

    struct EmptyParamCallback
    {
        inline constexpr bool operator()()
        {
            return false;
        }
    };

    struct EmptyResultCallback
    {
        inline void operator()(){}
    };

    /**
     * @brief Builds prepared statement from query string and constructs prepared stament with updatable arguments
     * @param query Query to be executed (in most cases there will be selections)
     * @param connection Connection handle into database
     * @param paramsSetter function setting statement's parameters, returning bool if input data are available
     * @param processingFunction function to be invoked on every row
     *                           Its parameters must correspond with result columns (their types and count)
     * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
     * @tparam validateMode Indicates validate mode level
     * @tparam warnMode Indicates warning mode level
     * @tparam ignoreNullable Disables null type checking
     */
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
             typename ResultCallable,
             typename ParamCallable,
             typename ConnType>
    void psQuery(ConnType &&connection, const std::string &query, ParamCallable &&paramsSetter, ResultCallable &&processingFunction)
    {
        static constexpr bool hasParamCallback = !std::is_same<ParamCallable, EmptyParamCallback>::value;
        static constexpr bool hasResultCallback = !std::is_same<ResultCallable, EmptyResultCallback>::value;

        auto ps = detail::generatePreparedStatement<storeResult, validateMode, warnMode, ignoreNullable>(connection, query, paramsSetter, processingFunction);

        do {
            if (hasParamCallback)
            {
                if (!invokeViaTuple(paramsSetter, ps.getParams()))
                    break;

                ps.updateParamsBindings();
            }

            ps.execute();

            if (hasResultCallback)
            {
                while (ps.fetch())
                {
                    invokeViaTuple(processingFunction, ps.getResult());
                }
            }
        } while (hasParamCallback);
    }

    /**
     * @brief Executes query, reads input data and passes them row by row to callback function
     * @param ps Prepared statement object (only static statements are currently supported)
     * @param processingFunction function to be invoked on every row
     *                           Its parameters must correspond with result columns (their types and count)
     */
    template<typename PreparedStatementType, typename Callable, typename = typename std::enable_if<std::is_base_of<detail::StatementBase, std::remove_reference_t<PreparedStatementType>>::value>::type>
    [[deprecated("psReadQuery with already-created prepared statement is deprecated")]] void psReadQuery(PreparedStatementType &&ps, Callable &&processingFunction)
    {
        ps.execute();
        while (ps.fetch())
        {
            invokeViaTuple(processingFunction, ps.getResult());
        }
    }

    /**
     * @brief Helper function invoking psQuery without params setter
     * @param query Query to be executed (in most cases there will be selections)
     * @param connection Connection handle into database
     * @param processingFunction function to be invoked on every row
     *                           Its parameters must correspond with result columns (their types and count)
     * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
     * @tparam validateMode Indicates validate mode level
     * @tparam warnMode Indicates warning mode level
     * @tparam ignoreNullable Disables null type checking
     */
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
             typename Callable,
             typename ConnType>
    [[deprecated("psReadQuery is deprecated, use psResultQuery")]] void psReadQuery(const std::string &query, ConnType &&connection, Callable &&processingFunction)
    {
        psQuery<storeResult, validateMode, warnMode, ignoreNullable>(connection, query, EmptyParamCallback {}, processingFunction);
    }

    /**
     * @brief Helper function invoking psQuery without params setter
     * @param query Query to be executed (in most cases there will be selections)
     * @param connection Connection handle into database
     * @param processingFunction function to be invoked on every row
     *                           Its parameters must correspond with result columns (their types and count)
     * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
     * @tparam validateMode Indicates validate mode level
     * @tparam warnMode Indicates warning mode level
     * @tparam ignoreNullable Disables null type checking
     */
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
             typename Callable,
             typename ConnType>
    void psResultQuery(ConnType &&connection, const std::string &query, Callable &&processingFunction)
    {
        psQuery<storeResult, validateMode, warnMode, ignoreNullable>(connection, query, EmptyParamCallback {}, processingFunction);
    }

    /**
     * @brief Helper function invoking psQuery without processing function
     * @param query Query to be executed (in most cases there will be selections)
     * @param connection Connection handle into database
     * @param paramsSetter function setting statement's parameters, returning bool if input data are available
     * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
     * @tparam validateMode Indicates validate mode level
     * @tparam warnMode Indicates warning mode level
     * @tparam ignoreNullable Disables null type checking
     */
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
             typename Callable,
             typename ConnType>
    void psParamQuery(ConnType &&connection, const std::string &query, Callable &&paramsSetter)
    {
        psQuery<storeResult, validateMode, warnMode, ignoreNullable>(connection, query, paramsSetter, EmptyResultCallback {});
    }

    /**
     * @brief Reads one and only one row from your own prepared statement into variables
     * @param ps Prepared statement object (only static statements are currently supported)
     * @param values References to variables to be loaded from query
     *               Their type must be compatible with query result types
     * @throws UnexpectedRowCountError if number of results is not exactly 1
     */
    template<typename PreparedStatementType, typename... Args, typename = typename std::enable_if<std::is_base_of<detail::StatementBase, std::remove_reference_t<PreparedStatementType>>::value>::type>
    [[deprecated("psReadValues with already-created prepared statement is deprecated")]] void psReadValues(PreparedStatementType &&ps, Args&... values)
    {
        detail::psReadValuesImpl(ps, values...);
    }

    /**
     * @brief Reads one and only one row from prepared statement (constructed from `query` string) into variables
     * @param query Query to be executed (Ensure you are returning only 1 row)
     * @param connection Connection handle into database
     * @param values Reference to variables to be loaded from query
     *               Their type must be compatible with query result types
     * @tparam storeResult Boolean indicating if results will be in `store` or `use` mode
     * @tparam validateMode Indicates validate mode level
     * @tparam warnMode Indicates warning mode level
     * @tparam ignoreNullable Disables null type checking
     */
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
             typename... Args,
             typename ConnType>
    void psReadValues(const std::string &query, ConnType &&connection, Args&... values)
    {
        detail::psReadValuesImpl(connection.template makePreparedStatement<ResultBindings<Args...>, storeResult, validateMode, warnMode, ignoreNullable>(query), values...);
    }
}

