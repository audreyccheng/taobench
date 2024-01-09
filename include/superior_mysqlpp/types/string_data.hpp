/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <algorithm>
#include <string>
#include <utility>
#include <cstring>

#include <superior_mysqlpp/types/array.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/types/blob_data.hpp>
#include <superior_mysqlpp/converters.hpp>
#include <superior_mysqlpp/exceptions.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>


namespace SuperiorMySqlpp
{
    /**
     * Note: string types are always zero-terminated
     */
    template<std::size_t N>
    class StringDataBase : public BlobDataBase<N, true>
    {
    public:
        constexpr StringDataBase() = default;
        constexpr StringDataBase(const StringDataBase&) = default;
        constexpr StringDataBase(StringDataBase&&) = default;
        StringDataBase& operator=(const StringDataBase&) = default;
        StringDataBase& operator=(StringDataBase&&) = default;
        ~StringDataBase() = default;

        using BlobDataBase<N, true>::BlobDataBase;


        /*
         * TODO: add optional arguments specifying precision and formating
         */
        template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type=0>
        explicit StringDataBase(T value)
        {
            auto s = Converters::toString(value);
            if (s.size() > this->maxSize())
            {
                throw RuntimeError{"Conversion truncated data!"};
            }

            std::memcpy(this->data(), s.data(), s.size());
            this->itemsCount = s.size();
        }

        constexpr auto length() const
        {
            return this->size();
        }

        template<typename T, typename=std::enable_if_t<std::is_arithmetic<T>::value>>
        T to() const
        {
            return Converters::to<T>(this->data(), length());
        }

        template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type=0>
        explicit operator T() const
        {
            return to<T>();
        }
    };

    using StringData = StringDataBase<1024>;


    namespace detail
    {
        template<std::size_t N>
        struct CanBindAsParam<BindingTypes::String, StringDataBase<N>> : std::true_type {};

        template<std::size_t N>
        struct CanBindAsResult<BindingTypes::String, StringDataBase<N>> : std::true_type {};
    }
}

