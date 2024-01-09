/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <string>
#include <vector>
#include <sstream>

#include <superior_mysqlpp/connection_def.hpp>
#include <superior_mysqlpp/query_result.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/uncaught_exception_counter.hpp>


namespace SuperiorMySqlpp
{
    class Query
    {
    private:
        LowLevel::DBDriver& driver;
        std::string query;
        bool quoteNext = false;
        bool escapeNext = false;
        bool executed = false;
        UncaughtExceptionCounter uncaughtExceptionCounter{};

    private:
        void resetNextState()
        {
            quoteNext = false;
            escapeNext = false;
        }

    public:
        Query(LowLevel::DBDriver& driver)
            : driver{driver}, query{}
        {
        }

        Query(LowLevel::DBDriver& driver, const std::string& initialQuery)
            : driver{driver}, query{initialQuery}
        {
        }

        Query(Connection& connection)
            : driver{connection.detail_getDriver()}, query{}
        {
        }

        Query(Connection& connection, const std::string& initialQuery)
            : driver{connection.detail_getDriver()}, query{initialQuery}
        {
        }

        Query(Query&& other)
            : driver{std::move(other).driver},
              query{std::move(other).query},
              quoteNext{std::move(other).quoteNext},
              escapeNext{std::move(other).escapeNext},
              executed{std::move(other).executed},
              uncaughtExceptionCounter{std::move(other).uncaughtExceptionCounter}
        {
            // We must ensure that other will not do multi-statement checks in destructor
            other.executed = false;
        }

        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
        Query& operator=(Query&&) = delete;

        ~Query() noexcept(false)
        {

            if (!uncaughtExceptionCounter.isNewUncaughtException())
            {
                // C API requires to read all result sets.
                if (executed && hasMoreResults())
                {
                    throw LogicError{"Some result sets had not been read!"};
                }
            }
            else
            {
                // This check has no logical sense when there is an active exception.
            }
        }

        std::string getQueryString() const
        {
            return query;
        }

        const StringView getQueryStringView() const
        {
            return query;
        }

        StringView getQueryStringView()
        {
            return query;
        }


        auto affectedRows()
        {
            if (!executed)
            {
                throw QueryNotExecuted{"Can not get affected rows. Query has not yet been executed!"};
            }

            return driver.affectedRows();
        }

        std::string info()
        {
            if (!executed)
            {
                throw QueryNotExecuted{"Can not get query info. Query has not yet been executed!"};
            }

            return driver.queryInfo();
        }

        auto getInsertId()
        {
            return driver.getInsertId();
        }

        std::string str() const
        {
            return query;
        }

        void execute()
        {
            driver.execute(query);
            executed = true;
        }

        bool hasMoreResults()
        {
            if (!executed)
            {
                throw QueryNotExecuted{"Can not ask if statement has more results. Query has not yet been executed!"};
            }

            return driver.hasMoreResults();
        }

        bool nextResult()
        {
            // FIXME: free previous result set

            if (!executed)
            {
                throw QueryNotExecuted{"Can not ask for next result. Query has not yet been executed!"};
            }

            return driver.nextResult();
        }

        void nextResultOrFail()
        {
            if (!nextResult())
            {
                throw LogicError{"There are no more results!"};
            }
        }

        // TODO: add support for template queries (like printf)
        // (do type check {parse format string} in compile time)
//        template<typename... Args>
//        void execute(Args... args) = delete;

        auto store()
        {
            if (!executed)
            {
                throw QueryNotExecuted{"Can not store result. Query has not yet been executed!"};
            }

            return StoreQueryResult{driver.storeResult()};
        }

        StoreQueryResult storeNext()
        {
            nextResultOrFail();
            return store();
        }

        auto use()
        {
            if (!executed)
            {
                throw QueryNotExecuted{"Can not use result. Query has not yet been executed!"};
            }

            return UseQueryResult{driver.useResult()};
        }

        UseQueryResult useNext()
        {
            nextResultOrFail();
            return use();
        }

        template<typename F>
        void forEachRow(F function)
        {
            UseQueryResult result{use()};
            while (Row row{result.fetchRow()})
            {
                function(row);
            }
        }

        /*
         * TODO: Implement functions below.
         * (Find column type, call to<type>(), validate metadata.
         * Allow user specializations using "to<>" or operator T()".)
         */

//        template<typename Container, typename F>
//        void storeIf(Container& container, F&& function) = delete;

//        template<typename Sequence>
//        void storeInSequence(Sequence& container) = delete;

        // TODO: storeIn specialize for vector, set, ...


        operator std::string() const
        {
            return query;
        }

        template<typename Integer>
        std::enable_if_t<std::is_arithmetic<Integer>::value, Query>& operator<<(Integer value)
        {
            if (quoteNext)
            {
                query.append("'");
            }

            query.append(std::to_string(value));

            if (quoteNext)
            {
                query.append("'");
            }

            resetNextState();

            return *this;
        }

        template<typename T>
        std::enable_if_t<!std::is_arithmetic<T>::value, Query>& operator<<(const T& value)
        {
            if (quoteNext)
            {
                query.append("'");
            }

            if (escapeNext)
            {
                query.append(driver.escapeString(value));
            }
            else
            {
                query.append(value);
            }

            if (quoteNext)
            {
                query.append("'");
            }

            resetNextState();

            return *this;
        }

        Query& operator<<(Query& (*functionPtr)(Query&))
        {
            return functionPtr(*this);
        }

        friend Query& escape(Query&);
        friend Query& quoteOnly(Query&);
        friend Query& quote(Query&);
    };


    inline Query& escape(Query& query)
    {
        query.escapeNext = true;
        return query;
    }

    inline Query& quoteOnly(Query& query)
    {
        query.quoteNext = true;
        return query;
    }

    inline Query& quote(Query& query)
    {
        query.escapeNext = true;
        query.quoteNext = true;
        return query;
    }
}

