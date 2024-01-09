/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <mysql/mysql.h>

#include <ostream>

#include <superior_mysqlpp/converters.hpp>
#include <superior_mysqlpp/types/string_view.hpp>


namespace SuperiorMySqlpp
{
    class Field
    {
    private:
        char* stringData;
        unsigned long stringLength;

    public:

        Field(char* stringData, unsigned long stringLength)
            : stringData{stringData}, stringLength{stringLength}
        {
        }

        Field(Field&&) = default;

        Field(const Field&) = delete;
        Field& operator=(const Field&) = delete;
        Field& operator=(Field&&) = delete;

        auto data() const noexcept
        {
            return stringData;
        }

        auto length() const noexcept
        {
            return stringLength;
        }

        auto size() const noexcept
        {
            return stringLength;
        }

        auto begin() const noexcept
        {
            return stringData;
        }

        auto cbegin() const noexcept
        {
            return begin();
        }

        auto end() const noexcept
        {
            return stringData + stringLength;
        }

        auto cend() const noexcept
        {
            return end();
        }

        auto getString() const
        {
            return std::string(stringData, stringLength);
        }

        StringView getStringView() const
        {
            return {stringData, stringLength};
        }

        bool isNull() const noexcept
        {
            return stringData == nullptr;
        }

        operator bool() const noexcept
        {
            return stringData != nullptr;
        }

        template<typename T>
        T to() const
        {
            return Converters::to<T>(data(), length());
        }

        template<typename T>
        explicit operator T() const
        {
            return to<T>();
        }
    };


    /*
     * operator==
     */
    inline bool operator==(const Field& first, const Field& second)
    {
        return first.getStringView() == second.getStringView();
    }

    inline bool operator==(const Field& field, const std::string& string)
    {
        return field.getStringView() == string;
    }

    inline bool operator==(const Field& field, const StringView& stringView)
    {
        return field.getStringView() == stringView;
    }

    // reverse combinations
    template<typename T>
    inline bool operator==(const T& something, const Field& field)
    {
        return field == something;
    }


    /*
     * operator!=
     */
    inline bool operator!=(const Field& first, const Field& second)
    {
        return first.getStringView() != second.getStringView();
    }

    inline bool operator!=(const Field& first, const std::string& second)
    {
        return first.getStringView() != second;
    }

    inline bool operator!=(const Field& field, const StringView& stringView)
    {
        return field.getStringView() != stringView;
    }

    // reverse combinations
    template<typename T>
    inline bool operator!=(const T& something, const Field& field)
    {
        return field != something;
    }


    /*
     * operator<<
     */
    inline std::ostream& operator<<(std::ostream& os, const Field& field)
    {
        os << field.data();
        return os;
    }
}
