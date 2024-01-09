/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <mysql/mysql.h>

#include <superior_mysqlpp/types/time.hpp>
#include <superior_mysqlpp/types/date.hpp>
#include <superior_mysqlpp/types/datetime.hpp>
#include <superior_mysqlpp/types/nullable.hpp>
#include <superior_mysqlpp/types/array.hpp>
#include <superior_mysqlpp/types/blob_data.hpp>
#include <superior_mysqlpp/types/string_data.hpp>
#include <superior_mysqlpp/types/decimal_data.hpp>


namespace SuperiorMySqlpp
{
    namespace Sql
    {
        using TinyInt = signed char;
        using SmallInt = short int;
        using MediumInt = int;
        using Int = int;
        using BigInt = long long int;

        using UTinyInt = std::make_unsigned_t<TinyInt>;
        using USmallInt = std::make_unsigned_t<SmallInt>;
        using UMediumInt = std::make_unsigned_t<MediumInt>;
        using UInt = std::make_unsigned_t<Int>;
        using UBigInt = std::make_unsigned_t<BigInt>;


        using Float = float;
        using Double = double;


        using Time = Time;
        using Date = Date;
        using Datetime = Datetime;
        using Timestamp = Timestamp;


        template<typename T>
        using Nullable = Nullable<T>;


        template<std::size_t N>
        using ArrayBase = ArrayBase<N>;

        using Blob = BlobData;
        template<std::size_t N>
        using BlobBase = BlobDataBase<N>;

        using String = StringData;
        template<std::size_t N>
        using StringBase = StringDataBase<N>;

        using Decimal = DecimalData;
        template<std::size_t N>
        using DecimalBase = DecimalDataBase<N>;
    }
}
