/*
 * Author: Tomas Nozicka
 */

#pragma once


// TODO: change 201701L when optional will become part of standard
#if __cplusplus >= 201701L
#include <string_view>
namespace SuperiorMySqlpp
{
    using StringView = std::string_view;
}
#else
#include <experimental/string_view>
namespace SuperiorMySqlpp
{
    using StringView = std::experimental::string_view;
    namespace StringViewLiterals = std::experimental::string_view_literals;
}
#endif
