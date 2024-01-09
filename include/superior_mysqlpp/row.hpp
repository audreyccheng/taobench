/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <mysql/mysql.h>

#include <iosfwd>

#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/field.hpp>
#include <superior_mysqlpp/iterator.hpp>


namespace SuperiorMySqlpp
{
    class Row
    {
    private:
        MYSQL_ROW mysqlRow;
        unsigned long* fieldsLengthsPtr;
        const unsigned int fieldsCount;

    public:
        Row(LowLevel::DBDriver::Result& result, MYSQL_ROW mysqlRow, unsigned int fieldsCount)
            : mysqlRow{mysqlRow},
              fieldsLengthsPtr{result.fetchLengths()},
              fieldsCount{fieldsCount}
        {
        }

        Row(Row&&) = default;

        Row(const Row&) = delete;
        Row& operator=(const Row&) = delete;
        Row& operator=(Row&&) = delete;



        /*
         * TODO: add iterators throughout fields
         */
        class Iterator : public detail::Iterator<MYSQL_ROW>
        {
        private:
            const Row& row;

        public:
            Iterator(MYSQL_ROW pointer, const Row& row)
                : detail::Iterator<MYSQL_ROW>{pointer}, row{row}
            {
            }

            Field operator*() const
            {
                return {*pointer, row.fieldsLengthsPtr[pointer-row.mysqlRow]};
            }
        };

        unsigned long size() const noexcept
        {
            return fieldsCount;
        }

        Iterator begin() const noexcept
        {
            return {mysqlRow, *this};
        }

        auto cbegin() const noexcept
        {
            return begin();
        }

        Iterator end() const noexcept
        {
            return {mysqlRow + fieldsCount, *this};
        }

        auto cend() const noexcept
        {
            return end();
        }

        operator bool() const noexcept
        {
            return mysqlRow != nullptr;
        }

        Field operator[](unsigned int index) const
        {
            return {mysqlRow[index], fieldsLengthsPtr[index]};
        }
    };



    inline std::ostream& operator<<(std::ostream& os, const Row& row)
    {
        os << "[";
        bool first = true;
        for (const auto& item: row)
        {
            if (first)
            {
                os << item.getStringView();
                first = false;
            }
            else
            {
                os << ", " << item.getStringView();
            }
        }
        os << "]";
        return os;
    }
}
