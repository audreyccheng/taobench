/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <superior_mysqlpp/types/string_data.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>


namespace SuperiorMySqlpp
{
    template<std::size_t N>
    class DecimalDataBase : public StringDataBase<N>
    {
    public:
        using StringDataBase<N>::StringDataBase;

        template<typename T>
        DecimalDataBase& operator=(T&& source)
        {
            StringDataBase<N>::operator=(std::forward<T>(source));
            return *this;
        }
    };

    /*
     * In MySQL DECIMAL shall have maximum of 65 digits + sign + decimal point
     */
    using DecimalData = DecimalDataBase<67>;


    namespace detail
    {
        template<std::size_t N>
        struct CanBindAsParam<BindingTypes::Decimal, DecimalDataBase<N>> : std::true_type {};

        template<std::size_t N>
        struct CanBindAsResult<BindingTypes::Decimal, DecimalDataBase<N>> : std::true_type {};
    }
}

