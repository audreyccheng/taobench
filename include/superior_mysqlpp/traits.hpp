/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <type_traits>


namespace SuperiorMySqlpp { namespace Traits
{
    template <typename...>
    struct Voider
    {
        using type = void;
    };

    template <typename... Ts>
    using Void_t = typename Voider<Ts...>::type;



    template<typename...>
    struct JustType;


    template<typename, typename, typename, typename...>
    struct ReplaceTypeImpl;

    template<typename... ResultArgs, typename From, typename To, typename... Args>
    struct ReplaceTypeImpl<JustType<ResultArgs...>, From, To, Args...>
    {
        using Type = JustType<ResultArgs...>;
    };

    template<typename... ResultArgs, typename From, typename To, typename Head, typename... Tail>
    struct ReplaceTypeImpl<JustType<ResultArgs...>, From, To, Head, Tail...>
    {
        using Type = typename ReplaceTypeImpl<JustType<ResultArgs..., Head>, From, To, Tail...>::Type;
    };

    template<typename... ResultArgs, typename From, typename To, typename... Tail>
    struct ReplaceTypeImpl<JustType<ResultArgs...>, From, To, From, Tail...>
    {
        using Type = typename ReplaceTypeImpl<JustType<ResultArgs..., To>, From, To, Tail...>::Type;
    };


    template<template<typename...> class, typename>
    struct ReplaceTypeHelper;

    template<template<typename...> class Result, typename... Args>
    struct ReplaceTypeHelper<Result, JustType<Args...>>
    {
        using Type = Result<Args...>;
    };

    template<template<typename...> class Result, typename From, typename To, typename... Args>
    struct ReplaceType
    {
        using Type = typename ReplaceTypeHelper<Result, typename ReplaceTypeImpl<JustType<>, From, To, Args...>::Type>::Type;
    };

    template<template<typename...> class Result, typename From, typename To, typename... Args>
    using ReplaceType_t = typename ReplaceType<Result, From, To, Args...>::Type;



    namespace detail
    {
        template<typename T, typename U, typename... Args>
        struct IsContained : IsContained<T, Args...> {};

        template<typename T, typename... Args>
        struct IsContained<T, T, Args...> : std::true_type {};

        template<typename T, typename U>
        struct IsContained<T, U> : std::false_type {};

        template<typename T>
        struct IsContained<T, T> : std::true_type {};
    }

    template<typename T, typename... Args>
    struct IsContained : detail::IsContained<T, Args...> {};

    template<typename T>
    struct IsContained<T> : std::false_type {};
}}

