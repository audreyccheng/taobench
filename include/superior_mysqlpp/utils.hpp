/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <string>
#include <tuple>
#include <utility>


namespace SuperiorMySqlpp
{
    template<typename T, template <typename, typename=std::allocator<T>> class Container>
    inline std::string toString(Container<T> container)
    {
        std::string result{};
        std::size_t i= 0;
        for (auto&& item: container)
        {
            result += std::to_string(item);
            if (++i != container.size())
            {
                result += ", ";
            }
        }

        return result;
    }

    namespace detail
    {
        /**
         * @brief Invokes function via unpacked tuple
         * @param f Functor to be invoked
         * @param t Tuple of arguments to be unpacked
         */
        template<typename Function, typename Tuple, std::size_t... I>
        auto invokeViaTupleImpl(Function f, Tuple &&t, std::index_sequence<I...>)
        {
            return f(std::get<I>(t)...);
        }
    }

    /**
     * @brief Invokes function via unpacked tuple
     * @param f Functor to be invoked
     * @param t Tuple of arguments to be unpacked
     */
    template<typename Function, typename Tuple>
    auto invokeViaTuple(Function f, Tuple &&t)
    {
        constexpr std::size_t tuple_size = std::tuple_size<std::decay_t<Tuple>>::value;
        return detail::invokeViaTupleImpl(f, std::forward<Tuple>(t), std::make_index_sequence<tuple_size>{});
    }

    /**
     * Writes variable number of arguments into arbitrary std(-like) stream.
     * @param stream Stream reference.
     * @param arg Perfectly forwarded argument
     * @return Stream reference
     */
    template <typename Stream, typename Arg>
    Stream &streamify(Stream &stream, Arg &&arg)
    {
        return stream << std::forward<Arg>(arg);
    }

    /**
     * @overload
     */
    template <typename Stream, typename Arg, typename... Args>
    Stream &streamify(Stream &stream, Arg &&arg, Args&&...args)
    {
        return streamify(streamify(stream, arg), std::forward<Args>(args)...);
    }

    namespace detail
    {
        /**
         * Function information flags
         */
        enum : uint64_t
        {
            FunctionFlag_IsConst = 1ULL << 0ULL,
            FunctionFlag_IsVolatile = 1ULL << 1ULL,
            FunctionFlag_ThisIsLvalueRef = 1ULL << 3ULL,
            FunctionFlag_ThisIsRvalueRef = 1ULL << 4ULL
        };

        /**
         * @brief Generic function info obtained from FunctionInfo trait
         * 
         * @tparam ResultType type returned by function
         * @tparam IsConst true_type, if function has been marked const
         * @tparam IsVolatile true_type, if function has been marked volatile
         * @tparam Args function arguments
         */
        template<typename ResultType, uint64_t Flags, typename... Args>
        struct FunctionInfoImpl
        {
            using result_type   = ResultType;
            using is_const      = std::integral_constant<bool, (Flags & FunctionFlag_IsConst) != 0>;
            using is_volatile   = std::integral_constant<bool, (Flags & FunctionFlag_IsVolatile) != 0>;
            using is_lvalue_ctx = std::integral_constant<bool, (Flags & FunctionFlag_ThisIsLvalueRef) != 0>;
            using is_rvalue_ctx = std::integral_constant<bool, (Flags & FunctionFlag_ThisIsRvalueRef) != 0>;
            using arguments     = std::tuple<Args...>;

            template<template<typename...> class Trait>
            using transform_args = Trait<Args...>;
        };
    }

    /**
     * Detects, if all template parameters are references
     * 
     * @tparam Args... Types to be checked
     */
    template<typename... Args>
    struct AreArgumentsLvalueRefs : std::false_type {};

    /**
     * Detects, if all template parameters are references
     *
     * @tparam Args... Types to be checked
     */
    template<typename... Args>
    struct AreArgumentsLvalueRefs<Args&...> : std::true_type {};

    /**
     * Removes any const/volatile/reference from Args
     * 
     * @tparam Args... Types to be checked
     */
    template<typename... Args>
    struct RemoveCVRefArgs
    {
        using type = std::tuple<std::remove_cv_t<std::remove_reference_t<Args>>...>;
    };

    /**
     * Type trait providing metadata about functions.
     * Accepts functions, methods and functors (types with operator()).
     * 
     * @tparam T function prototype
     */
    template<typename T>
    struct FunctionInfo;

    /**
     * @brief information about std::function<>-like declaration (e.g. `void(int, int)`)
     * 
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Args...)> : detail::FunctionInfoImpl<ResultType, 0, Args...> {};

    /**
     * @brief Plain old C function overload
     * 
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (*)(Args...)> : detail::FunctionInfoImpl<ResultType, 0, Args...> {};

    /**
     * @brief Class member function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...)> : detail::FunctionInfoImpl<ResultType, 0, Args...> {};

    /**
     * @brief Class const-member function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst, Args...> {};

    /**
     * @brief Class volatile-member function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) volatile> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsVolatile, Args...> {};

    /**
     * @brief Class const-volatile-member function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const volatile> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst | detail::FunctionFlag_IsVolatile, Args...> {};

    /**
     * @brief Class member lvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) &> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_ThisIsLvalueRef, Args...> {};

    /**
     * @brief Class const-member lvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const &> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst | detail::FunctionFlag_ThisIsLvalueRef, Args...> {};

    /**
     * @brief Class volatile-member lvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) volatile &> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsVolatile | detail::FunctionFlag_ThisIsLvalueRef, Args...> {};

    /**
     * @brief Class const-volatile-member lvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const volatile &> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst | detail::FunctionFlag_IsVolatile | detail::FunctionFlag_ThisIsLvalueRef, Args...> {};

    /**
     * @brief Class member rvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) &&> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_ThisIsRvalueRef, Args...> {};

    /**
     * @brief Class const-member rvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const &&> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst | detail::FunctionFlag_ThisIsRvalueRef, Args...> {};

    /**
     * @brief Class volatile-member rvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) volatile &&> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsVolatile | detail::FunctionFlag_ThisIsRvalueRef, Args...> {};

    /**
     * @brief Class const-volatile-member rvalue-ref-context function overload
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename Class, typename ResultType, typename... Args>
    struct FunctionInfo<ResultType (Class::*)(Args...) const volatile &&> : detail::FunctionInfoImpl<ResultType, detail::FunctionFlag_IsConst | detail::FunctionFlag_IsVolatile | detail::FunctionFlag_ThisIsRvalueRef, Args...> {};

    /**
     * @brief Lambda overload (by decaying its type to class member function)
     * 
     * @tparam Class Class of member
     * @tparam ResultType type returned by function
     * @tparam Args function arguments
     */
    template<typename T>
    struct FunctionInfo : FunctionInfo<decltype(&std::decay_t<T>::operator())> {};

    /**
     * @brief Calls function if Evaluate is true
     *
     * @tparam Evaluate Flag if Invokable should be invoked
     */
    template<bool Evaluate>
    struct CompileTimeIf {
        template<typename Invokable>
        static inline void then(Invokable &&) noexcept
        {
        }
    };

    /**
     * @brief Calls function if Evaluate is true
     *
     * @tparam Evaluate Flag if Invokable should be invoked
     *
     * @overload
     */
    template<>
    struct CompileTimeIf<true> {
        template<typename Invokable>
        static inline void then(Invokable &&invokable) noexcept(noexcept(std::declval<std::decay_t<Invokable>>()()))
        {
            invokable();
        }
    };
}
