/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <superior_mysqlpp/prepared_statements/get_binding_type.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>
#include <superior_mysqlpp/types/blob_data.hpp>
#include <superior_mysqlpp/types/string_data.hpp>
#include <superior_mysqlpp/types/decimal_data.hpp>
#include <superior_mysqlpp/types/nullable.hpp>

namespace SuperiorMySqlpp
{
    /*
     * Objective of this whole file is proper initialization of bindings.
     * Namely, filling of MYSQL_BIND structures that tell the C MySQL client what
     * type given parameter/result field has and where is it stored.
     *
     * There are following points of interest from the outside code:
     *  #initializeParamBinding and #initializeResultBinding
     *      - initializes single concrete binding to a storage (mutable lvalue reference) of given type T
     *      - specializations exist for various kind of data - integers, floats, strings etc.
     *      - entry points from DynamicPreparedStatement
     *  #initializeBindings
     *      - initializes whole binding storage tuple from SuperiorMySql::Binding.
     *      - entry point used from PreparedStatement
     *      - through template machinery just calls initializeParamBinding/initializeResultBinding as appropriate
     */
    namespace detail
    {
        /**
         * Trait specialization saying that std::string can be used as parameter
         * for string field type.
         */
        template<>
        struct CanBindAsParam<BindingTypes::String, std::string> : std::true_type {};


        /**
         * Initializes binding if the storage is SuperiorMySqlpp::Nullable,
         * usage is shared between initialization code of param and result bindings.
         */
        template<typename T>
        constexpr inline void initializeNullable(MYSQL_BIND& binding, Nullable<T>& nullable)
        {
            static_assert(sizeof(decltype(binding.is_null)) == sizeof(decltype(&nullable.detail_getNullRef())),
                "Pointers to null indicators must have same size.");
            static_assert(sizeof(decltype(*binding.is_null)) == sizeof(decltype(nullable.detail_getNullRef())),
                "Representations of boolean null indicators must be equivalent in SuperiorMysqlpp and C client backend.");
            binding.is_null = reinterpret_cast<my_bool*>(&nullable.detail_getNullRef());
        }

        /**
         * Param binding initialization specialization for integers.
         */
        template<typename T>
        constexpr inline std::enable_if_t<std::is_integral<T>::value>
        initializeParamBinding(MYSQL_BIND& binding, T& value)
        {
            using PureType_t = std::decay_t<T>;
            using Signed_t = std::make_signed_t<PureType_t>;

            binding.buffer = &value;
            binding.buffer_type = detail::toMysqlEnum(getBindingType<Signed_t>());
            binding.is_unsigned = std::is_unsigned<PureType_t>::value;
        }

        /**
         * Param binding initialization specialization for floats.
         */
        template<typename T>
        constexpr inline std::enable_if_t<std::is_floating_point<T>::value>
        initializeParamBinding(MYSQL_BIND& binding, T& value)
        {
            using PureType_t = std::decay_t<T>;

            // Underlying C client (and SQL itself) supports only float and double floating point types.
            static_assert(!std::is_same<PureType_t, long double>::value,
                    "'long double' is not supported by mysql prepared statements protocol!");

            binding.buffer = &value;
            binding.buffer_type = detail::toMysqlEnum(getBindingType<PureType_t>());
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type String.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::String, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& string)
        {
            binding.buffer = const_cast<void*>(reinterpret_cast<const void*>(string.data()));
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::String);
            binding.buffer_length = string.length();
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Decimal.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Decimal, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& decimal)
        {
            binding.buffer = decimal.data();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::NewDecimal);
            binding.buffer_length = decimal.length();
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Blob.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Blob, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& blob)
        {
            binding.buffer = blob.data();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Blob);
            binding.buffer_length = blob.size();
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Date.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Date, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& date)
        {
            binding.buffer = &date.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Date);
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Time.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Time, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& time)
        {
            binding.buffer = &time.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Time);
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Datetime.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Datetime, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& dateTime)
        {
            binding.buffer = &dateTime.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Datetime);
        }

        /**
         * Param binding initialization specialization for types bindable into SQL type Timestamp.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Timestamp, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& timestamp)
        {
            binding.buffer = &timestamp.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Timestamp);
        }

        /**
         * Param binding initialization specialization for C string.
         */
        inline void initializeParamBinding(MYSQL_BIND& binding, const char*& string)
        {
            binding.buffer = const_cast<void*>(reinterpret_cast<const void*>(string));
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::String);
            binding.buffer_length = std::strlen(string);
        }

        /**
         * Param binding initialization specialization for C fixed size array (treated as string).
         */
        template<std::size_t N>
        constexpr inline void initializeParamBinding(MYSQL_BIND& binding, const char (&string)[N])
        {
            binding.buffer = const_cast<void*>(reinterpret_cast<const void*>(string));
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::String);
            static_assert(N>0, "The Universe is falling apart :(");
            binding.buffer_length = N-1;
        }

        /**
         * Param binding initialization specialization bindable into SuperiorMySqlpp::Nullable.
         */
        template<typename T, typename std::enable_if<CanBindAsParam<BindingTypes::Nullable, T>::value, int>::type=0>
        inline void initializeParamBinding(MYSQL_BIND& binding, T& nullable)
        {
            initializeNullable(binding, nullable);
            // Initializing nullable internals is not needed for empty case
            if (!*binding.is_null)
            {
                initializeParamBinding(binding, *nullable);
            }
        }


        /**
         * Result binding initialization for arithmentic types (ints and floats).
         * Shares implementation with param bindings.
         * @remark Refactoring note - while this function can share implementation with ParamBinding version,
         *          you cannnot do the same for functions guarded by CanBindAsParam (as the mirrored
         *          version may not exist).
         */
        template<typename T>
        inline std::enable_if_t<std::is_arithmetic<T>::value>
        initializeResultBinding(MYSQL_BIND& binding, T& value)
        {
            initializeParamBinding(binding, value);
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type String.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::String, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& string)
        {
            binding.buffer = string.data();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::String);
            binding.buffer_length = string.maxSize();
            binding.length = &string.counterRef();
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Decimal
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Decimal, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& decimal)
        {
            binding.buffer = decimal.data();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::NewDecimal);
            binding.buffer_length = decimal.maxSize();
            binding.length = &decimal.counterRef();
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Blob.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Blob, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& blob)
        {
            binding.buffer = blob.data();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Blob);
            binding.buffer_length = blob.maxSize();
            binding.length = &blob.counterRef();
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Date.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Date, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& date)
        {
            binding.buffer = &date.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Date);
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Time.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Time, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& time)
        {
            binding.buffer = &time.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Time);
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Datetime.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Datetime, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& datetime)
        {
            binding.buffer = &datetime.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Datetime);
        }

        /**
         * Result binding initialization specialization for types bindable into SQL type Timestamp.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Timestamp, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& timestamp)
        {
            binding.buffer = &timestamp.detail_getBufferRef();
            binding.buffer_type = detail::toMysqlEnum(FieldTypes::Timestamp);
        }

        /**
         * Result binding initialization specialization for types bindable into SuperiorMySqlpp::Nullable.
         */
        template<typename T, typename std::enable_if<CanBindAsResult<BindingTypes::Nullable, T>::value, int>::type=0>
        inline void initializeResultBinding(MYSQL_BIND& binding, T& nullable)
        {
            initializeNullable(binding, nullable);
            // Note that initialization of payload is here unconditional, unlike in initializeParamBinding version
            initializeResultBinding(binding, *nullable);
        }


        /**
         * Initializes individual binding.
         * Generic template, actual implementation is in specializations.
         * @tparam isParamBinding Is true if individual binding is used for params, not results or given statement's query.
         */
        template<bool isParamBinding>
        struct InitializeBindingImpl;

        /**
         * Specialization of #InitializeBindingImpl for query parameters.
         */
        template<>
        struct InitializeBindingImpl<true>
        {
            template<typename Data>
            static void call(MYSQL_BIND& binding, Data& data)
            {
                initializeParamBinding(binding, data);
            }
        };

        /**
         * Specialization of #InitializeBindingImpl for query results.
         */
        template<>
        struct InitializeBindingImpl<false>
        {
            template<typename Data>
            static void call(MYSQL_BIND& binding, Data& data)
            {
                initializeResultBinding(binding, data);
            }
        };


        /**
         * Initialization of Nth binding of SuperiorMySqlpp::Bindings (recursive template).
         */
        template<bool isParamBinding, int N>
        struct InitializeBindingsImpl
        {
            template<typename Bindings, typename Data>
            static void call(Bindings& bindings, Data& data)
            {
                InitializeBindingImpl<isParamBinding>::call(bindings[N-1], std::get<N-1>(data));
                InitializeBindingsImpl<isParamBinding, N-1>::call(bindings, data);
            }
        };

        /**
         * Specialization of #InitializeBindingsImpl for first binding.
         */
        template<bool isParamBinding>
        struct InitializeBindingsImpl<isParamBinding, 1>
        {
            template<typename Bindings, typename Data>
            static void call(Bindings& bindings, Data& data)
            {
                InitializeBindingImpl<isParamBinding>::call(bindings[0], std::get<0>(data));
            }
        };

        /**
         * Specialization of InitializeBindingsImpl for empty bindings.
         */
        template<bool isParamBinding>
        struct InitializeBindingsImpl<isParamBinding, 0>
        {
            template<typename Bindings, typename Data>
            static void call(Bindings&, Data&)
            {
            }
        };


        /**
         * Initializes Bindings, both for ParamBindings and ResultBindings.
         * Calls respectively initializeParamBinding or initializeResultBinding for each field.
         * @tparam isParamBinding Is true if binding is a ParamBindings specialization.
         * @param bindings Type #Bindings<...>.
         * @param data Type std::tuple (or any heterogenous ordered container supporting std::get).
         */
        template<bool isParamBinding, typename... Types, typename Bindings, typename Data>
        inline void initializeBindings(Bindings& bindings, Data& data)
        {
            InitializeBindingsImpl<isParamBinding, sizeof...(Types)>::call(bindings, data);
        }
    }
}
