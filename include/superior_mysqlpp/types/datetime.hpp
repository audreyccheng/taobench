/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <utility>

#include <superior_mysqlpp/types/mysql_time.hpp>
#include <superior_mysqlpp/types/date.hpp>
#include <superior_mysqlpp/types/time.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>


namespace SuperiorMySqlpp
{
    /**
     * Class Datetime is used for values that contain both date and time parts and is bound to MYSQL_TYPE_DATETIME.
     * The supported range is '1000-01-01 00:00:00.000000' to '9999-12-31 23:59:59.999999'.
     * 
     * @see https://dev.mysql.com/doc/refman/5.7/en/datetime.html
     */
    class Datetime : public detail::MysqlTime, public detail::DateBase<Datetime>, public detail::TimeBase<Datetime>
    {
        
        /**
         * Allowing access to protected atributes. 
         */
        friend class detail::DateBase<Datetime>;
        friend class detail::TimeBase<Datetime>;

    public:
        
        /**
         * Default constructor.
         */
        Datetime() = default;

        /**
         * Constructor with MYSQL_TIME values.
         * 
         * @param year Year. 
         * @param month Month.
         * @param day Day.
         * @param hour Hour.
         * @param minute Minute.
         * @param second Second.
         * @param secondFraction Fraction of second.
         */
        Datetime(Year_t year, Month_t month, Day_t day, Hour_t hour=0, Minute_t minute=0, Second_t second=0, SecondFraction_t secondFraction=0)
        {
            setYear(year);
            setMonth(month);
            setDay(day);
            setHour(hour);
            setMinute(minute);
            setSecond(second);
            setSecondFraction(secondFraction);
        }

        /**
         * Constructor with Date and Time values.
         * 
         * @param date Object of type Date. May be in format {Year_t year, Month_t month, Day_t day}.
         * @param time Object of type Time. May be in format {Hour_t hour, Minute_t minute, Second_t second}.
         * @param secondFraction Fraction of second.
         */
        Datetime(Date date, Time time, SecondFraction_t secondFraction=0)
        {
            setDate(std::move(date));
            setTime(std::move(time));
            setSecondFraction(secondFraction);
        }

        Datetime(const Datetime&) = default;
        Datetime(Datetime&&) = default;
        Datetime& operator=(const Datetime&) = default;
        Datetime& operator=(Datetime&&) = default;
        
        /**
         * Default destructor.
         */
        ~Datetime() = default;

        /**
         * Returns the fraction of second stored in Datetime.
         */
        auto getSecondFraction() const
        {
            return buffer.second_part;
        }

        /**
         * Sets the fraction of second stored in Datetime.
         */
        template<typename U>
        void setSecondFraction(U&& secondFraction)
        {
            buffer.second_part = std::forward<U>(secondFraction);
        }

        /**
         * Returns the year, month and day stored in Datetime.
         * 
         * @return Object of type Date.
         */
        Date getDate() const
        {
            return {getYear(), getMonth(), getDay()};
        }

        /**
         * Sets the date stored in Datetime.
         */
        template<typename U>
        void setDate(U&& value)
        {
            setYear(std::forward<U>(value).getYear());
            setMonth(std::forward<U>(value).getMonth());
            setDay(std::forward<U>(value).getDay());
        }

        /**
         * Returns the hour, minute and second stored in Datetime.
         * 
         * @return Object of type Time.
         */
        Time getTime() const
        {
            return {getHour(), getMinute(), getSecond()};
        }

        /**
         * Sets the time stored in Datetime.
         */
        template<typename U>
        void setTime(U&& value)
        {
            setHour(std::forward<U>(value).getHour());
            setMinute(std::forward<U>(value).getMinute());
            setSecond(std::forward<U>(value).getSecond());
        }
    };


    namespace detail
    {
        /**
         * Says that object Datetime can be used as a storage of type datetime as a variant for query parameter.
         */
        template<>
        struct CanBindAsParam<BindingTypes::Datetime, Datetime> : std::true_type {};

        /**
         * Says that object Datetime can be used as a storage of type datetime as a variant for query result.
         */
        template<>
        struct CanBindAsResult<BindingTypes::Datetime, Datetime> : std::true_type {};
    }



    /**
     * Timestamp class is used for values that contain both date and time parts and is bound to MYSQL_TYPE_TIMESTAMP.
     * MySQL converts Timestamp values from the current time zone to UTC for storage, and back from UTC to the current time zone for retrieval.
     * The supported range is '1970-01-01 00:00:01.000000' to '2038-01-19 03:14:07.999999'.
     *
     * @see https://dev.mysql.com/doc/refman/5.7/en/datetime.html
     */
    class Timestamp : public Datetime
    {
        using Datetime::Datetime;
    };


    namespace detail
    {
        
        /**
         * Says that object Timestamp can be used as a storage of type timestamp as a variant for query parameter.
         */
        template<>
        struct CanBindAsParam<BindingTypes::Timestamp, Timestamp> : std::true_type {};

        /**
         * Says that object Timestamp can be used as a storage of type timestamp as a variant for query result.
         */
        template<>
        struct CanBindAsResult<BindingTypes::Timestamp, Timestamp> : std::true_type {};
    }



    /**
     * Equality operator of two objects of type Datetime.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if both objects represent the same date, time and fraction of second, false if not.
     */
    inline bool operator==(const Datetime& lhs, const Datetime& rhs)
    {
        return lhs.getDate()==rhs.getDate() &&
               lhs.getTime()==rhs.getTime() &&
               lhs.getSecondFraction()==rhs.getSecondFraction();
    }

    /**
     * Inequality operator of two objects of type Datetime.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if at least date, time or fraction of second is not same in both objects, false if they all are the same.
     */
    inline bool operator!=(const Datetime& lhs, const Datetime& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * Less than operator of two Datetime objects.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if parameter lhs represents earlier date and time than parameter rhs, false if not.
     */
    inline bool operator<(const Datetime& lhs, const Datetime& rhs)
    {
        if (lhs.getDate() < rhs.getDate())
        {
            return true;
        }
        else if (lhs.getDate() > rhs.getDate())
        {
            return false;
        }

        if (lhs.getTime() < rhs.getTime())
        {
            return true;
        }
        else if (lhs.getTime() > rhs.getTime())
        {
            return false;
        }

        return lhs.getSecondFraction() < rhs.getSecondFraction();
    }

    /**
     * Less than or equal operator of two Datetime objects.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if parameter lhs represents earlier or same date and time as parameter rhs, false if otherwise.
     */
    inline bool operator<=(const Datetime& lhs, const Datetime& rhs)
    {
        if (lhs.getDate() < rhs.getDate())
        {
            return true;
        }
        else if (lhs.getDate() > rhs.getDate())
        {
            return false;
        }

        if (lhs.getTime() < rhs.getTime())
        {
            return true;
        }
        else if (lhs.getTime() > rhs.getTime())
        {
            return false;
        }

        return lhs.getSecondFraction() <= rhs.getSecondFraction();
    }

    /**
     * Greater than operator of two Datetime objects.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if parameter lhs represents later date and time than parameter rhs, false if not.
     */
    inline bool operator>(const Datetime& lhs, const Datetime& rhs)
    {
        if (lhs.getDate() > rhs.getDate())
        {
            return true;
        }
        else if (lhs.getDate() < rhs.getDate())
        {
            return false;
        }

        if (lhs.getTime() > rhs.getTime())
        {
            return true;
        }
        else if (lhs.getTime() < rhs.getTime())
        {
            return false;
        }

        return lhs.getSecondFraction() > rhs.getSecondFraction();
    }

    /**
     * Greater than or equal operator of two Datetime objects.
     * 
     * @param lhs Object of type Datetime.
     * @param rhs Object of type Datetime.
     * @return True if parameter lhs represents later or same date and time as parameter rhs, false if otherwise.
     */
    inline bool operator>=(const Datetime& lhs, const Datetime& rhs)
    {
        if (lhs.getDate() > rhs.getDate())
        {
            return true;
        }
        else if (lhs.getDate() < rhs.getDate())
        {
            return false;
        }

        if (lhs.getTime() > rhs.getTime())
        {
            return true;
        }
        else if (lhs.getTime() < rhs.getTime())
        {
            return false;
        }

        return lhs.getSecondFraction() >= rhs.getSecondFraction();
    }
}

