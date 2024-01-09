/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <superior_mysqlpp/connection_def.hpp>
#include <superior_mysqlpp/query.hpp>
#include <superior_mysqlpp/traits.hpp>
#include <superior_mysqlpp/types/tags.hpp>
#include <superior_mysqlpp/dynamic_prepared_statement.hpp>


namespace SuperiorMySqlpp
{
    template<typename RBindings,
             bool storeResult,
             ValidateMetadataMode validateMode,
             ValidateMetadataMode warnMode,
             bool ignoreNullable,
             typename... Args>
    PreparedStatement<RBindings,
                      ParamBindings<std::decay_t<Args>...>,
                      storeResult,
                      validateMode,
                      warnMode,
                      ignoreNullable
                      > Connection::makePreparedStatement(const std::string& query, Args&&... args) &
    {
        return {*this, query, std::forward<Args>(args)...};
    }


    template<typename RBindings,
             bool storeResult,
             ValidateMetadataMode validateMode,
             ValidateMetadataMode warnMode,
             bool ignoreNullable,
             template<typename...> class RArgsTuple, template<typename...> class PArgsTuple,
             typename... RArgs, typename... PArgs>
    PreparedStatement<RBindings,
                      ParamBindings<std::decay_t<PArgs>...>,
                      storeResult,
                      validateMode,
                      warnMode,
                      ignoreNullable
                      > Connection::makePreparedStatement(const std::string& query,
                                                          FullInitTag tag, RArgsTuple<RArgs...>&& resultArgs,
                                                          PArgsTuple<PArgs...>&& paramArgs) &
    {
        return {*this, query, tag, std::forward<RArgsTuple<RArgs...>>(resultArgs), std::forward<PArgsTuple<PArgs...>>(paramArgs)};
    }


    template<bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable>
    DynamicPreparedStatement<storeResult, validateMode, warnMode, ignoreNullable> Connection::makeDynamicPreparedStatement(const std::string& query) &
    {
        return {*this, query};
    }

    template<typename... Args>
    Query Connection::makeQuery(Args&&... args) &
    {
        return {*this, std::forward<Args>(args)...};
    }
}
