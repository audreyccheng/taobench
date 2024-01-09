/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <utility>
#include <cstring>
#include <algorithm>

#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/row.hpp>
#include <superior_mysqlpp/metadata.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/types/optional.hpp>


namespace SuperiorMySqlpp
{
    namespace detail
    {
        class QueryResult
        {
        protected:
            unsigned int fieldsCount;
            ResultMetadata resultMetadata;
            Optional<std::vector<std::tuple<StringView, std::size_t>>> optionalColumnNameMappings;

        protected:
            LowLevel::DBDriver::Result& getResult() && = delete;
            LowLevel::DBDriver::Result& getResult() const && = delete;

            LowLevel::DBDriver::Result& getResult() &
            {
                return resultMetadata.detail_getResult();
            }

            const LowLevel::DBDriver::Result& getResult() const &
            {
                return resultMetadata.detail_getResult();
            }

            auto& getColumnNameMappingsRef()
            {
                if (!optionalColumnNameMappings)
                {
                    optionalColumnNameMappings.emplace();
                    optionalColumnNameMappings->reserve(resultMetadata.size());

                    std::size_t i = 0;
                    for (auto&& item: resultMetadata)
                    {
                        optionalColumnNameMappings->emplace_back(std::make_tuple(item.getColumnNameView(), i++));
                    }

                    using std::begin;
                    using std::end;
                    std::sort(begin(*optionalColumnNameMappings), end(*optionalColumnNameMappings));
                }

                return *optionalColumnNameMappings;
            }

        public:
            explicit QueryResult(LowLevel::DBDriver::Result&& result)
                : fieldsCount{result.getFieldsCount()},
                  resultMetadata{std::move(result)}
            {
            }

            QueryResult(QueryResult&&) = default;

            QueryResult(const QueryResult&) = delete;
            QueryResult& operator=(const QueryResult&) = delete;
            QueryResult& operator=(QueryResult&&) = delete;

            Row fetchRow() && = delete;

            ResultMetadata& getMetadata() && = delete;
            ResultMetadata& getMetadata() const && = delete;

            ResultMetadata& getMetadata() &
            {
                return resultMetadata;
            }

            const ResultMetadata& getMetadata() const &
            {
                return resultMetadata;
            }


            /*
             * Also does implicit conversions form std::string and const char*
             */
            std::size_t getColumnIndex(StringView columnName)
            {
                auto& columnNameMappings = getColumnNameMappingsRef();

                using std::begin;
                using std::end;
                auto it = std::lower_bound(begin(columnNameMappings), end(columnNameMappings), columnName, [](auto&& elem, auto&& value){
                    return std::get<0>(elem) < value;
                });

                if (it != end(columnNameMappings) && !(columnName < std::get<0>(*it)))
                {
                    return std::get<1>(*it);
                }
                else
                {
                    throw OutOfRange{"Column name \"" + std::string(columnName) + "\" not found!"};
                }
            }

            /*
             * Must be separate otherwise const char[N] will decay to const char* and string_view constructor will compute length
             */
            template<std::size_t L>
            std::size_t getColumnIndex(const char (& columnName)[L])
            {
                static_assert(L>0, "");
                return getColumnIndex({columnName, L-1});
            }



            /*
             * DO NOT USE this function unless you want to work with C API directly!
             */
            LowLevel::DBDriver::Result& detail_getResult() && = delete;
            LowLevel::DBDriver::Result& detail_getResult() const && = delete;

            LowLevel::DBDriver::Result& detail_getResult() &
            {
                return getResult();
            }

            const LowLevel::DBDriver::Result& detail_getResult() const &
            {
                return getResult();
            }
        };
    }


    class UseQueryResult : public detail::QueryResult
    {
    public:
        using detail::QueryResult::QueryResult;

        UseQueryResult(UseQueryResult&&) = default;

        UseQueryResult(const UseQueryResult&) = delete;
        UseQueryResult& operator=(const UseQueryResult&) = delete;
        UseQueryResult& operator=(UseQueryResult&&) = delete;

        auto getFetchedRowsCount()
        {
            return getResult().getRowsCount();
        }

        Row fetchRow() &
        {
            auto mysqlRow = getResult().checkedFetchRow();
            return {getResult(), mysqlRow, fieldsCount};
        }
    };


    class StoreQueryResult : public detail::QueryResult
    {
    public:
        using detail::QueryResult::QueryResult;

        StoreQueryResult(StoreQueryResult&&) = default;

        StoreQueryResult(const StoreQueryResult&) = delete;
        StoreQueryResult& operator=(const StoreQueryResult&) = delete;
        StoreQueryResult& operator=(StoreQueryResult&&) = delete;

        auto getRowsCount()
        {
            return getResult().getRowsCount();
        }

        template<typename T>
        void seekRow(T index)
        {
            getResult().seekRow(index);
        }

        void seekRowOffset(MYSQL_ROW_OFFSET offset)
        {
            getResult().seekRowOffset(offset);
        }

        auto tellRowOffset()
        {
            return getResult().tellRowOffset();
        }

        Row fetchRow() &
        {
            auto mysqlRow = getResult().fetchRow();
            return {getResult(), mysqlRow, fieldsCount};
        }
    };
}
