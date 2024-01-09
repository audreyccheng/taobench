/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <utility>

#include <superior_mysqlpp/types/mysql_time.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>


namespace SuperiorMySqlpp
{
    namespace detail
    {
        template<typename T>
        class DateBase
        {
        public:
            auto getYear() const
            {
                return static_cast<T*>(const_cast<DateBase*>(this))->buffer.year;
            }

            template<typename U>
            void setYear(U&& year)
            {
                static_cast<T*>(this)->buffer.year = std::forward<U>(year);
            }

            auto getMonth() const
            {
                return static_cast<T*>(const_cast<DateBase*>(this))->buffer.month;
            }

            template<typename U>
            void setMonth(U&& month)
            {
                static_cast<T*>(this)->buffer.month = std::forward<U>(month);
            }

            auto getDay() const
            {
                return static_cast<T*>(const_cast<DateBase*>(this))->buffer.day;
            }

            template<typename U>
            void setDay(U&& day)
            {
                static_cast<T*>(this)->buffer.day = std::forward<U>(day);
            }
        };
    }

    class Date : public detail::MysqlTime, public detail::DateBase<Date>
    {
        friend class detail::DateBase<Date>;

    public:
        Date() = default;

        template<typename Year, typename Month, typename Day>
        Date(Year&& year, Month&& month, Day&& day)
        {
            setYear(std::forward<Year>(year));
            setMonth(std::forward<Month>(month));
            setDay(std::forward<Day>(day));
        }

        Date(const Date&) = default;
        Date(Date&&) = default;
        Date& operator=(const Date&) = default;
        Date& operator=(Date&&) = default;
        ~Date() = default;
    };


    namespace detail
    {
        template<>
        struct CanBindAsParam<BindingTypes::Date, Date> : std::true_type {};

        template<>
        struct CanBindAsResult<BindingTypes::Date, Date> : std::true_type {};
    }



    inline bool operator==(const Date& lhs, const Date& rhs)
    {
        return lhs.getYear() == rhs.getYear() &&
               lhs.getMonth() == rhs.getMonth() &&
               lhs.getDay() == rhs.getDay();
    }

    inline bool operator!=(const Date& lhs, const Date& rhs)
    {
        return !(lhs == rhs);
    }

    inline bool operator<(const Date& lhs, const Date& rhs)
    {
        if (lhs.getYear() < rhs.getYear())
        {
            return true;
        }
        else if (lhs.getYear() > rhs.getYear())
        {
            return false;
        }

        if (lhs.getMonth() < rhs.getMonth())
        {
            return true;
        }
        else if (lhs.getMonth() > rhs.getMonth())
        {
            return false;
        }

        return lhs.getDay() < rhs.getDay();
    }

    inline bool operator<=(const Date& lhs, const Date& rhs)
    {
        if (lhs.getYear() < rhs.getYear())
        {
            return true;
        }
        else if (lhs.getYear() > rhs.getYear())
        {
            return false;
        }

        if (lhs.getMonth() < rhs.getMonth())
        {
            return true;
        }
        else if (lhs.getMonth() > rhs.getMonth())
        {
            return false;
        }

        return lhs.getDay() <= rhs.getDay();
    }

    inline bool operator>(const Date& lhs, const Date& rhs)
    {
        if (lhs.getYear() > rhs.getYear())
        {
            return true;
        }
        else if (lhs.getYear() < rhs.getYear())
        {
            return false;
        }

        if (lhs.getMonth() > rhs.getMonth())
        {
            return true;
        }
        else if (lhs.getMonth() < rhs.getMonth())
        {
            return false;
        }

        return lhs.getDay() > rhs.getDay();
    }

    inline bool operator>=(const Date& lhs, const Date& rhs)
    {
        if (lhs.getYear() > rhs.getYear())
        {
            return true;
        }
        else if (lhs.getYear() < rhs.getYear())
        {
            return false;
        }

        if (lhs.getMonth() > rhs.getMonth())
        {
            return true;
        }
        else if (lhs.getMonth() < rhs.getMonth())
        {
            return false;
        }

        return lhs.getDay() >= rhs.getDay();
    }
}

