/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <mysql/mysql.h>
#include <cstring>


namespace SuperiorMySqlpp { namespace detail
{
    class MysqlTime
    {
    protected:
        MYSQL_TIME buffer;

    public:
        using Year_t = decltype(buffer.year);
        using Month_t = decltype(buffer.month);
        using Day_t = decltype(buffer.day);
        using Hour_t = decltype(buffer.hour);
        using Minute_t = decltype(buffer.minute);
        using Second_t = decltype(buffer.second);
        using SecondFraction_t = decltype(buffer.second_part);


        auto& detail_getBufferRef()
        {
            return buffer;
        }
    };
}}

