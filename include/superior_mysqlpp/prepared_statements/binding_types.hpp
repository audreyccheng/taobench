/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <type_traits>



namespace SuperiorMySqlpp
{
    namespace detail
    {
        /**
         * Enumeration of various kinds of bindings.
         * Integral and floating types are ommited, those types
         * are referred to by appropriate primitive C type instead.
         */
        enum class BindingTypes
        {
            Nullable,
            String,
            Decimal,
            Blob,
            Date,
            Time,
            Datetime,
            Timestamp,
        };

        /**
         * This boolean trait describes if type T can be used as a storage for
         * a given binding type (of enum type #BindingTypes).
         * This is a variant for query parameters and defaults to false.
         */
        template<BindingTypes, typename T>
        struct CanBindAsParam : std::false_type {};

        /**
         * This boolean trait describes if type T can be used as a storage for
         * a given binding type (of enum type #BindingTypes).
         * This is a variant for query results and defaults to false.
         */
        template<BindingTypes, typename T>
        struct CanBindAsResult: std::false_type {};
    }
}
