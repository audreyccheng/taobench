/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <tuple>


namespace SuperiorMySqlpp
{
    /**
     * Generic template and default implementation of initialization of storage for one field of result.
     * Uses static method approach as templated specialization is required.
     * This common default variant does not need to do anything, but advanced types like SuperiorMySqlpp::Nullable do.
     */
    template<typename DecayedType>
    struct InitializeResultItemImpl
    {
        template<typename T>
        static void call(T&&)
        {
        }
    };

    /**
     * Generic template and default of implementation of initialization of storage for one field of result.
     * This auxiliary function only serves to decay type and call the actual implementation.
     * @param value Value to initialize result from.
     */
    template<typename T>
    void intializeResultItem(T&& value)
    {
        InitializeResultItemImpl<std::decay_t<T>>::call(value);
    }



    /*
     * Following code calls intializeResultItem for each tuple member.
     */
    namespace detail
    {
        /**
         * Initialize storage for single result field - implementation.
         */
        template<int N>
        struct InitializeResultImpl
        {
            template<typename Data>
            static void call(Data& data)
            {
                intializeResultItem(std::get<N-1>(data));
                InitializeResultImpl<N-1>::call(data);
            }
        };

        /**
         * Specialization of #InitializeResultImpl for case 0.
         */
        template<>
        struct InitializeResultImpl<0>
        {
            template<typename Data>
            static void call(Data&)
            {
            }
        };
    }


    /**
     * Initializes std::tuple from result bindings - storage backing the result of prepared statement.
     * As SuperiorMySqlpp::DynamicPreparedStatement have each field stored separately, this is
     * only useful for SuperiorMySqlpp::PreparedStatement.
     */
    template<typename... Types>
    void InitializeResult(std::tuple<Types...>& data)
    {
        detail::InitializeResultImpl<sizeof...(Types)>::call(data);
    }
}

