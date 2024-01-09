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
        class TimeBase
        {
        public:
            auto getHour() const
            {
                return static_cast<T*>(const_cast<TimeBase*>(this))->buffer.hour;
            }

            template<typename U>
            void setHour(U&& hour)
            {
                static_cast<T*>(this)->buffer.hour = std::forward<U>(hour);
            }

            auto getMinute() const
            {
                return static_cast<T*>(const_cast<TimeBase*>(this))->buffer.minute;
            }

            template<typename U>
            void setMinute(U&& minute)
            {
                static_cast<T*>(this)->buffer.minute = std::forward<U>(minute);
            }

            auto getSecond() const
            {
                return static_cast<T*>(const_cast<TimeBase*>(this))->buffer.second;
            }

            template<typename U>
            void setSecond(U&& second)
            {
                static_cast<T*>(this)->buffer.second = std::forward<U>(second);
            }
        };
    }

    class Time : public detail::MysqlTime, public detail::TimeBase<Time>
    {
        friend class detail::TimeBase<Time>;

    public:
        Time() = default;
        template<typename Hour, typename Minute, typename Second>
        Time(Hour&& hour, Minute&& minute, Second&& second, bool negative=false)
            : detail::MysqlTime{}
        {
            setHour(std::forward<Hour>(hour));
            setMinute(std::forward<Minute>(minute));
            setSecond(std::forward<Second>(second));
            setSign(negative);
        }

        Time(const Time&) = default;
        Time(Time&&) = default;
        Time& operator=(const Time&) = default;
        Time& operator=(Time&&) = default;
        ~Time() = default;

        bool isNegative() const
        {
            return buffer.neg;
        }

        void setSign(bool negative)
        {
            buffer.neg = negative;
        }

        void setNegative()
        {
            setSign(true);
        }

        void setPositive()
        {
            setSign(false);
        }
    };


    namespace detail
    {
        template<>
        struct CanBindAsParam<BindingTypes::Time, Time> : std::true_type {};

        template<>
        struct CanBindAsResult<BindingTypes::Time, Time> : std::true_type {};
    }



    inline bool operator==(const Time& lhs, const Time& rhs)
    {
        return lhs.getHour() == rhs.getHour() &&
               lhs.getMinute() == rhs.getMinute() &&
               lhs.getSecond() == rhs.getSecond();
    }

    inline bool operator!=(const Time& lhs, const Time& rhs)
    {
        return !(lhs == rhs);
    }

    inline bool operator<(const Time& lhs, const Time& rhs)
    {
        if (lhs.getHour() < rhs.getHour())
        {
            return true;
        }
        else if (lhs.getHour() > rhs.getHour())
        {
            return false;
        }

        if (lhs.getMinute() < rhs.getMinute())
        {
            return true;
        }
        else if (lhs.getMinute() > rhs.getMinute())
        {
            return false;
        }

        return lhs.getSecond() < rhs.getSecond();
    }

    inline bool operator<=(const Time& lhs, const Time& rhs)
    {
        if (lhs.getHour() < rhs.getHour())
        {
            return true;
        }
        else if (lhs.getHour() > rhs.getHour())
        {
            return false;
        }

        if (lhs.getMinute() < rhs.getMinute())
        {
            return true;
        }
        else if (lhs.getMinute() > rhs.getMinute())
        {
            return false;
        }

        return lhs.getSecond() <= rhs.getSecond();
    }

    inline bool operator>(const Time& lhs, const Time& rhs)
    {
        if (lhs.getHour() > rhs.getHour())
        {
            return true;
        }
        else if (lhs.getHour() < rhs.getHour())
        {
            return false;
        }

        if (lhs.getMinute() > rhs.getMinute())
        {
            return true;
        }
        else if (lhs.getMinute() < rhs.getMinute())
        {
            return false;
        }

        return lhs.getSecond() > rhs.getSecond();
    }

    inline bool operator>=(const Time& lhs, const Time& rhs)
    {
        if (lhs.getHour() > rhs.getHour())
        {
            return true;
        }
        else if (lhs.getHour() < rhs.getHour())
        {
            return false;
        }

        if (lhs.getMinute() > rhs.getMinute())
        {
            return true;
        }
        else if (lhs.getMinute() < rhs.getMinute())
        {
            return false;
        }

        return lhs.getSecond() >= rhs.getSecond();
    }
}

