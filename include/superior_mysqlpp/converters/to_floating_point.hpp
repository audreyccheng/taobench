/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <cstdlib>
#include <stdexcept>


namespace SuperiorMySqlpp { namespace Converters
{
    /*
     * TODO: write optimized conversion functions
     */

    namespace detail
    {
    }

    template<typename T>
    inline T toFloatingPoint(const char* str, std::size_t length) = delete;


    template<>
    inline float toFloatingPoint(const char* str, std::size_t length)
    {
        if (length == 0)
        {
            throw std::domain_error{"SuperiorMysqlpp::toFloatingPoint"};
        }
        return std::strtof(str, nullptr);
    }

    template<>
    inline double toFloatingPoint(const char* str, std::size_t length)
    {
        if (length == 0)
        {
            throw std::domain_error{"SuperiorMysqlpp::toFloatingPoint"};
        }
        return std::strtod(str, nullptr);
    }

    template<>
    inline long double toFloatingPoint(const char* str, std::size_t length)
    {
        if (length == 0)
        {
            throw std::domain_error{"SuperiorMysqlpp::toFloatingPoint"};
        }
        return std::strtold(str, nullptr);
    }
}}
