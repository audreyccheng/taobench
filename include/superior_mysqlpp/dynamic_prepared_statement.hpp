/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <superior_mysqlpp/dynamic_prepared_statement_fwd.hpp>

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cstring>
#include <vector>
#include <cinttypes>

#include <superior_mysqlpp/prepared_statements/initialize_bindings.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>
#include <superior_mysqlpp/prepared_statements/prepared_statement_base.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/connection_def.hpp>
#include <superior_mysqlpp/types/nullable.hpp>

namespace SuperiorMySqlpp
{
    template<bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable>
    class DynamicPreparedStatement : public detail::PreparedStatementBase<storeResult, validateMode, warnMode, ignoreNullable>
    {
    private:
        unsigned int paramsCount = 0;
        int resultFieldCount = -1;

        std::vector<MYSQL_BIND> paramsBindings;
        std::vector<MYSQL_BIND> resultBindings;

        template<typename T>
        void bindResultInternal(unsigned int index, T& value)
        {
            throwIfStatementNotExecuted();
            if (index >= resultBindings.size())
            {
                throw OutOfRange("Param result index " + std::to_string(index) + " >= bindings size " + std::to_string(resultBindings.size()));
            }
            detail::initializeResultBinding(resultBindings[index], value);
        }

        void throwIfStatementNotExecuted() const
        {
            if (resultFieldCount < 0)
            {
                throw DynamicPreparedStatementLogicError("Statement has not yet been executed!");
            }
        }

    public:
        DynamicPreparedStatement(LowLevel::DBDriver& driver, const std::string& query)
            : detail::PreparedStatementBase<storeResult, validateMode, warnMode, ignoreNullable>{driver.makeStatement()}
        {
            this->statement.prepare(query);

            paramsCount = this->statement.paramCount();
            if (paramsCount > 0)
            {
                paramsBindings.resize(paramsCount);
            }
        }

        DynamicPreparedStatement(Connection& connection, const std::string& query)
            : DynamicPreparedStatement{connection.detail_getDriver(), query}
        {
        }

        DynamicPreparedStatement(const DynamicPreparedStatement&) = default;
        DynamicPreparedStatement(DynamicPreparedStatement&&) = default;
        DynamicPreparedStatement& operator=(const DynamicPreparedStatement&) = default;
        DynamicPreparedStatement& operator=(DynamicPreparedStatement&&) = default;

        ~DynamicPreparedStatement() = default;


        auto getParamsCount() const noexcept
        {
            return paramsCount;
        }

        unsigned int getResultFieldCount() const
        {
            throwIfStatementNotExecuted();
            return resultFieldCount;
        }

        template<typename T>
        void bindParam(unsigned int index, T& value)
        {
            if (index >= paramsBindings.size())
            {
                throw OutOfRange("Param binding index " + std::to_string(index) + " >= bindings size " + std::to_string(paramsBindings.size()));
            }
            detail::initializeParamBinding(paramsBindings[index], value);
        }

        template<typename T>
        void bindResult(unsigned int index, T& value)
        {
            bindResultInternal(index, value);
        }

        template<typename T>
        void bindResult(unsigned int index, Nullable<T>& value)
        {
            bindResultInternal(index, value);
            this->nullableBindings.push_back(&value);
        }

        void updateParamBindings()
        {
            if (paramsCount > 0)
            {
                this->statement.bindParam(paramsBindings.data());
            }
        }

        void updateResultBindings()
        {
            throwIfStatementNotExecuted();

            if (resultFieldCount > 0)
            {
                this->validateResultMetadata(resultBindings);
                this->statement.bindResult(resultBindings.data());
            }
        }

        void execute()
        {
            this->statement.execute();

            resultFieldCount = this->statement.fieldCount();

            if (resultFieldCount > 0)
            {
                resultBindings.resize(resultFieldCount);
            }

            this->storeOrUse();

            this->nullableBindings.clear();
        }

        bool nextResult()
        {
            this->statement.freeResult();
            return this->statement.nextResult();
        }

        void nextResultOrFail()
        {
            if (!nextResult())
            {
                throw LogicError{"There are no more results!"};
            }
        }
    };
}

