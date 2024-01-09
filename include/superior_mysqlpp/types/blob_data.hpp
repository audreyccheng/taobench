/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <algorithm>
#include <string>
#include <cstring>
#include <cassert>

#include <superior_mysqlpp/types/array.hpp>
#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>


namespace SuperiorMySqlpp
{
    template<std::size_t N, bool TerminatingZero = false>
    class BlobDataBase : public ArrayBase<N, TerminatingZero>
    {
    private:
        void assign(const char* const& data, std::size_t size)
        {
            auto updateSize = std::min(size, N);
            std::memcpy(this->array.data(), data, updateSize);
            this->itemsCount = updateSize;
            if (TerminatingZero)
            {
                *this->end() = '\0';
            }
        }

        void assign(const char* const& data)
        {
            assign(data, std::strlen(data));
        }

    public:
        constexpr BlobDataBase() = default;
        constexpr BlobDataBase(const BlobDataBase&) = default;
        constexpr BlobDataBase(BlobDataBase&&) = default;
        BlobDataBase& operator=(const BlobDataBase&) = default;
        BlobDataBase& operator=(BlobDataBase&&) = default;
        ~BlobDataBase() = default;

        BlobDataBase(const void* const& source, std::size_t size)
        {
            assign(source, size);
        }

        BlobDataBase(const char* const& source, std::size_t size)
        {
            assign(source, size);
        }

        /*
         * This constructor must be templated, otherwise it will always win in overload resolution over the one using array.
         * (They are both exact match, so the non-template one wins.)
         */
        template<typename T, typename std::enable_if<std::is_same<T, char>::value, int>::type=0>
        BlobDataBase(const T* const& source)
        {
            assign(source);
        }

        /**
         * Note: expects array to be zero terminated and thus discards last byte
         */
        template<std::size_t L>
        BlobDataBase(const char (& source)[L])
        {
            static_assert(L>0, "");
            assign(source, L-1);
        }

        BlobDataBase(const std::string& string)
        {
            assign(string.data(), string.size());
        }

        BlobDataBase(const StringView& string)
        {
            assign(string.data(), string.size());
        }


        std::string getString() const
        {
            return {this->array.data(), this->size()};
        }

        StringView getStringView() const
        {
            return {this->array.data(), this->size()};
        }

        operator std::string() const
        {
            return getString();
        }

        operator StringView() const
        {
            return getStringView();
        }
    };

    using BlobData = BlobDataBase<1024>;


    namespace detail
    {
        template<std::size_t N>
        struct CanBindAsParam<BindingTypes::Blob, BlobDataBase<N>> : std::true_type {};

        template<std::size_t N>
        struct CanBindAsResult<BindingTypes::Blob, BlobDataBase<N>> : std::true_type {};
    }


    template<std::size_t N, bool N_TZ, std::size_t M, bool M_TZ>
    bool operator==(const BlobDataBase<N, N_TZ>& lhs, const BlobDataBase<M, M_TZ>& rhs)
    {
        return lhs.getStringView() == rhs.getStringView();
    }

    template<std::size_t N, bool N_TZ>
    bool operator==(const std::string& lhs, const BlobDataBase<N, N_TZ>& rhs)
    {
        return lhs == rhs.getStringView();
    }

    template<std::size_t N, bool N_TZ>
    bool operator==(const BlobDataBase<N, N_TZ>& lhs, const std::string& rhs)
    {
        return lhs.getStringView() == rhs;
    }


    template<std::size_t N, bool N_TZ, std::size_t M, bool M_TZ>
    bool operator!=(const BlobDataBase<N, N_TZ>& lhs, const BlobDataBase<M, M_TZ>& rhs)
    {
        return lhs.getStringView() != rhs.getStringView();
    }

    template<std::size_t N, bool N_TZ>
    bool operator!=(const std::string& lhs, const BlobDataBase<N, N_TZ>& rhs)
    {
        return lhs == rhs.getStringView();
    }

    template<std::size_t N, bool N_TZ>
    bool operator!=(const BlobDataBase<N, N_TZ>& lhs, const std::string& rhs)
    {
        return lhs.getStringView() != rhs;
    }
}

