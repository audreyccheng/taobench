/*
 * Author: Tomas Nozicka
 */

#pragma once


// TODO: change 201701L when optional will become part of standard
#if __cplusplus >= 201701L
#include <optional>
namespace SuperiorMySqlpp
{
    template<typename T>
    using Optional = std::optional<T>;
    constexpr auto nullopt = std::nullopt;
}
#else
#include <experimental/optional>
namespace SuperiorMySqlpp
{
    template<typename T>
    using Optional = std::experimental::optional<T>;
    constexpr auto nullopt = std::experimental::nullopt;
}
#endif
