/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <type_traits>

#include <superior_mysqlpp/converters/to_integer.hpp>
#include <superior_mysqlpp/converters/to_floating_point.hpp>



namespace SuperiorMySqlpp { namespace Converters
{
    template<typename, typename=void>
    struct To;

    namespace detail {
        template<typename... Ts> struct make_void { typedef void type;};
        template<typename... Ts> using void_t = typename make_void<Ts...>::type;

        template<typename T>
        using get_to_method_type = decltype(&To<T>::operator());

        template<typename T, typename = void>
        struct has_to_overload : std::false_type {};

        template<typename T>
        struct has_to_overload<T, void_t<get_to_method_type<T>>> : std::true_type {};

    }

    template<typename T>
    struct To<T, std::enable_if_t<std::is_integral<T>::value>> {
        T operator()(const char* str, unsigned int length) {
            return toInteger<T>(str, length);
        }
    };

    template<typename T>
    struct To<T, std::enable_if_t<std::is_floating_point<T>::value>> {
        T operator()(const char* str, unsigned int length) {
            return toFloatingPoint<T>(str, length);
        }
    };

    template<typename T>
    struct To<T, std::enable_if_t<std::is_same<T, std::string>::value>> {
        T operator()(const char* str, unsigned int length) {
            return {str, length};
        }
    };

    /*
     * Support wrapper for old-style custom converters
     *
     * This wrapper is active only when appropriate `To` struct is defined (either here or by the user),
     * thus allowing the existing `to<T>` overloads to function as they did before and simlutaneously
     * enabling the user to define new `To` converters which can be defined with no respect to include order
     * (unlike the old-style way, which needed the specializations to be defined before the main header
     * inclusion)
     */
    template<typename T>
    inline std::enable_if_t<detail::has_to_overload<T>::value, T> to(const char* str, unsigned int length)
    {
        return To<T>{}(str, length);
    }

    template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type=0>
    inline std::string toString(T value)
    {
        return std::to_string(value);
    }

    template<typename T, typename std::enable_if<std::is_same<std::decay_t<T>, std::string>::value, int>::type=0>
    inline std::string toString(T&& value)
    {
        return std::forward<T>(value);
    }
}}
