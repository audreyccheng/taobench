/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <cinttypes>
#include <utility>
#include <limits>
#include <stdexcept>
#include <superior_mysqlpp/utils.hpp>

namespace SuperiorMySqlpp { 
/**
 * Namespace of converters designed for better performance.
 */    
namespace Converters
{
    namespace detail
    {
        /**
         * Calculates the power of number.
         * 
         * @tparam T Type of output and base.
         * @param x Base.
         * @param n Nonnegative exponent.
         * @return Power of x.
         */
        template <class T>
        inline constexpr T pow(T const& x, std::size_t n) {
            return n > 0 ? x * pow(x, n - 1) : 1;
        }

        /**
         * Parses the string into the number of given type.
         * 
         * @tparam T Type of result.
         * @tparam length Length of the string.
         * @tparam validate Whether to check if string is convertible.
         */
        template<typename T, int length, bool validate>
        struct ToIntegerParserImpl
        {
            /**
             * Check if length is not greater than is allowed for given type.
             */
            static_assert(std::numeric_limits<T>::digits10, "length > digits10");
            
            /**
             * Parses string into number of given type.
             * 
             * @param result Result to change recursively.
             * @param str String to be parsed.
             * @throws std::out_of_range exception if a character is not between 0 and 9.
             */
            static inline void call(T& result, const char*& str) noexcept(!validate)
            {
                static constexpr T base = pow(10UL, length-1);
                char c = *str;
                CompileTimeIf<validate>::then([c]()
                {
                    if (c < '0' || c > '9')
                    {
                        throw std::out_of_range("Character is not between 0 and 9!");
                    }
                });
                result += (c - '0') * base;
                ++str;
                ToIntegerParserImpl<T, length-1, validate>::call(result, str);
            }
        };

        /**
         * Parses the character into the number of given type.
         * 
         * @tparam T Type of result.
         * @tparam validate Whether to check if character is convertible.
         */
        template<typename T, bool validate>
        struct ToIntegerParserImpl<T, 1, validate>
        {
            static_assert(std::numeric_limits<T>::digits10, "length > digits10");

            /**
             * Parsers the character into the number of given type.
             * 
             * @param result Type of result.
             * @param str Character to be parsed.
             * @throws std::out_of_range exception if a character is not between 0 and 9.
             */
            static inline void call(T& result, const char*& str) noexcept(!validate)
            {
                char c = *str;
                CompileTimeIf<validate>::then([c]()
                {
                    if (c < '0' || c > '9')
                    {
                        throw std::out_of_range("Character is not between 0 and 9!");
                    }
                });
                result += (c - '0');
                ++str;
            }
        };

        /**
         * Eliminate undesired general conversion.
         */
        template<typename T, bool validate>
        struct ToIntegerUnsignedImpl
        {
            static inline T call(const char* str, unsigned int length) = delete;
        };

        /**
         * Converts string into uint8_t.
         * 
         * @tparam validate Check if string is convertible.
         */
        template<bool validate>
        struct ToIntegerUnsignedImpl<std::uint8_t, validate>
        {
            /**
             * Parses string to uint8_t.
             * 
             * @param str String to be converted.
             * @param length Length of the string.
             * @return Parsed integer value.
             * @throws std::runtime_error exception if length is not between 1 and 3.
             */
            static inline std::uint8_t call(const char* str, unsigned int length)
            {
                std::uint8_t result = 0;
                switch (length)
                {
                    case 3:
                        ToIntegerParserImpl<decltype(result), 3, validate>::call(result, str);
                        break;
                    case 2:
                        ToIntegerParserImpl<decltype(result), 2, validate>::call(result, str);
                        break;
                    case 1:
                        ToIntegerParserImpl<decltype(result), 1, validate>::call(result, str);
                        break;
                    default:
                        throw std::runtime_error("Internal error!");
                }
                return result;
            }
        };

        /**
         * Converts string into uint16_t.
         * 
         * @tparam validate Check if string is convertible.
         */
        template<bool validate>
        struct ToIntegerUnsignedImpl<std::uint16_t, validate>
        {
            /**
             * Parses string to uint16_t.
             * 
             * @param str String to be converted.
             * @param length Length of the string.
             * @return Parsed integer value.
             * @throws std::runtime_error exception if length is not between 1 and 5.
             */
            static inline std::uint16_t call(const char* str, unsigned int length)
            {
                std::uint16_t result = 0;
                switch (length)
                {
                    case 5:
                        ToIntegerParserImpl<decltype(result), 5, validate>::call(result, str);
                        break;
                    case 4:
                        ToIntegerParserImpl<decltype(result), 4, validate>::call(result, str);
                        break;
                    case 3:
                        ToIntegerParserImpl<decltype(result), 3, validate>::call(result, str);
                        break;
                    case 2:
                        ToIntegerParserImpl<decltype(result), 2, validate>::call(result, str);
                        break;
                    case 1:
                        ToIntegerParserImpl<decltype(result), 1, validate>::call(result, str);
                        break;
                    default:
                        throw std::runtime_error("Internal error!");
                }
                return result;
            }
        };

        /**
         * Converts string into uint32_t.
         * 
         * @tparam validate Check if string is convertible.
         */
        template<bool validate>
        struct ToIntegerUnsignedImpl<std::uint32_t, validate>
        {
            /**
             * Parses string to uint32_t.
             * 
             * @param str String to be converted.
             * @param length Length of the string.
             * @return Parsed integer value.
             * @throws std::runtime_error exception if length is not between 1 and 10.
             */
            static inline std::uint32_t call(const char* str, unsigned int length)
            {
                std::uint32_t result = 0;
                switch (length)
                {
                    case 10:
                        ToIntegerParserImpl<decltype(result), 10, validate>::call(result, str);
                        break;
                    case 9:
                        ToIntegerParserImpl<decltype(result), 9, validate>::call(result, str);
                        break;
                    case 8:
                        ToIntegerParserImpl<decltype(result), 8, validate>::call(result, str);
                        break;
                    case 7:
                        ToIntegerParserImpl<decltype(result), 7, validate>::call(result, str);
                        break;
                    case 6:
                        ToIntegerParserImpl<decltype(result), 6, validate>::call(result, str);
                        break;
                    case 5:
                        ToIntegerParserImpl<decltype(result), 5, validate>::call(result, str);
                        break;
                    case 4:
                        ToIntegerParserImpl<decltype(result), 4, validate>::call(result, str);
                        break;
                    case 3:
                        ToIntegerParserImpl<decltype(result), 3, validate>::call(result, str);
                        break;
                    case 2:
                        ToIntegerParserImpl<decltype(result), 2, validate>::call(result, str);
                        break;
                    case 1:
                        ToIntegerParserImpl<decltype(result), 1, validate>::call(result, str);
                        break;
                    default:
                        throw std::runtime_error("Internal error!");
                }
                return result;
            }
        };

        /**
         * Converts string into uint64_t.
         * 
         * @tparam validate Check if string is convertible.
         */
        template<bool validate>
        struct ToIntegerUnsignedImpl<std::uint64_t, validate>
        {
            /**
             * Parses string to uint64_t.
             * 
             * @param str String to be converted.
             * @param length Length of the string.
             * @return Parsed integer value.
             * @throws std::runtime_error exception if length is not between 1 and 20.
             */
            static inline std::uint64_t call(const char* str, unsigned int length)
            {
                std::uint64_t result = 0;
                switch (length)
                {
                    case 20:
                        ToIntegerParserImpl<decltype(result), 20, validate>::call(result, str);
                        break;
                    case 19:
                        ToIntegerParserImpl<decltype(result), 19, validate>::call(result, str);
                        break;
                    case 18:
                        ToIntegerParserImpl<decltype(result), 18, validate>::call(result, str);
                        break;
                    case 17:
                        ToIntegerParserImpl<decltype(result), 17, validate>::call(result, str);
                        break;
                    case 16:
                        ToIntegerParserImpl<decltype(result), 16, validate>::call(result, str);
                        break;
                    case 15:
                        ToIntegerParserImpl<decltype(result), 15, validate>::call(result, str);
                        break;
                    case 14:
                        ToIntegerParserImpl<decltype(result), 14, validate>::call(result, str);
                        break;
                    case 13:
                        ToIntegerParserImpl<decltype(result), 13, validate>::call(result, str);
                        break;
                    case 12:
                        ToIntegerParserImpl<decltype(result), 12, validate>::call(result, str);
                        break;
                    case 11:
                        ToIntegerParserImpl<decltype(result), 11, validate>::call(result, str);
                        break;
                    case 10:
                        ToIntegerParserImpl<decltype(result), 10, validate>::call(result, str);
                        break;
                    case 9:
                        ToIntegerParserImpl<decltype(result), 9, validate>::call(result, str);
                        break;
                    case 8:
                        ToIntegerParserImpl<decltype(result), 8, validate>::call(result, str);
                        break;
                    case 7:
                        ToIntegerParserImpl<decltype(result), 7, validate>::call(result, str);
                        break;
                    case 6:
                        ToIntegerParserImpl<decltype(result), 6, validate>::call(result, str);
                        break;
                    case 5:
                        ToIntegerParserImpl<decltype(result), 5, validate>::call(result, str);
                        break;
                    case 4:
                        ToIntegerParserImpl<decltype(result), 4, validate>::call(result, str);
                        break;
                    case 3:
                        ToIntegerParserImpl<decltype(result), 3, validate>::call(result, str);
                        break;
                    case 2:
                        ToIntegerParserImpl<decltype(result), 2, validate>::call(result, str);
                        break;
                    case 1:
                        ToIntegerParserImpl<decltype(result), 1, validate>::call(result, str);
                        break;
                    default:
                        throw std::runtime_error("Internal error!");
                }
                return result;
            }
        };

        /**
         * Parses string to unsigned number of given type.
         * 
         * @tparam T Type of result.
         * @tparam validate Check if string is convertible.
         * @param str String to be parsed.
         * @param length Length of the string.
         * @return Unsigned number of given type.
         */
        template<typename T, bool validate>
        inline T toIntegerUnsigned(const char* str, unsigned int length)
        {
            return ToIntegerUnsignedImpl<T, validate>::call(str, length);
        }
    }

    /**
     * Parses string to unsigned number of given type.
     * 
     * @tparam T Type of resut.
     * @tparam validate Check if string is convertible.
     * @param str String to be parsed.
     * @param length Length of the string.
     * @return Unsigned number of given type.
     */
    template<typename T, bool validate=false>
    inline std::enable_if_t<std::is_unsigned<T>::value, T> toInteger(const char* str, unsigned int length)
    {
        return detail::toIntegerUnsigned<T, validate>(str, length);
    }

    /**
     * Parses string to signed number of given type.
     * 
     * @tparam T Type of result.
     * @tparam validate Check if string is convertible.
     * @param str String to be parsed.
     * @param length Length of the string.
     * @return Signed number of given type.
     */
    template<typename T, bool validate=false>
    inline std::enable_if_t<std::is_signed<T>::value, T> toInteger(const char* str, unsigned int length)
    {
        bool isNegative = false;
        if (*str == '-')
        {
            isNegative = true;
            ++str;
            --length;
        }

        T result = toInteger<std::make_unsigned_t<T>, validate>(str, length);
        if (isNegative)
        {
            return result * -1;
        }
        else
        {
            return result;
        }
    }
}}
