/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <mysql/mysql.h>

#include <type_traits>
#include <stdexcept>


namespace SuperiorMySqlpp
{
    /**
     * Auxiliary enumeration of field types, used instead of #enum_field_types from
     * MySQL client, because it provides stronger type safety.
     */
    enum class FieldTypes : std::underlying_type_t<enum_field_types>
    {
        Tiny = MYSQL_TYPE_TINY,
        Short = MYSQL_TYPE_SHORT,
        Int24 = MYSQL_TYPE_INT24,
        Long = MYSQL_TYPE_LONG,
        LongLong = MYSQL_TYPE_LONGLONG,
        Float = MYSQL_TYPE_FLOAT,
        Double = MYSQL_TYPE_DOUBLE,
        Decimal = MYSQL_TYPE_DECIMAL,
        NewDecimal = MYSQL_TYPE_NEWDECIMAL,
        Time = MYSQL_TYPE_TIME,
        Date = MYSQL_TYPE_DATE,
        Datetime = MYSQL_TYPE_DATETIME,
        Timestamp = MYSQL_TYPE_TIMESTAMP,
        Year = MYSQL_TYPE_YEAR,
        String = MYSQL_TYPE_STRING,
        VarString = MYSQL_TYPE_VAR_STRING,
        Enum = MYSQL_TYPE_ENUM,
        Set = MYSQL_TYPE_SET,
        Bit = MYSQL_TYPE_BIT,
        Blob = MYSQL_TYPE_BLOB,
        TinyBlob = MYSQL_TYPE_TINY_BLOB,
        MediumBlob = MYSQL_TYPE_MEDIUM_BLOB,
        LongBlob = MYSQL_TYPE_LONG_BLOB,
        Geometry = MYSQL_TYPE_GEOMETRY,
        Null = MYSQL_TYPE_NULL,
    };

    /**
     * Conversion function from any type convertible to enum #FieldTypes to said type.
     * @param value Arbitrary value castable to FieldTypes.
     * @return Field type corresponding to function argument.
     */
    template<typename T>
    inline auto toFieldType(T value)
    {
        auto fieldType = static_cast<FieldTypes>(value);
        switch (fieldType)
        {
            case FieldTypes::Tiny:
            case FieldTypes::Short:
            case FieldTypes::Int24:
            case FieldTypes::Long:
            case FieldTypes::LongLong:
            case FieldTypes::Float:
            case FieldTypes::Double:
            case FieldTypes::Decimal:
            case FieldTypes::NewDecimal:
            case FieldTypes::Time:
            case FieldTypes::Date:
            case FieldTypes::Datetime:
            case FieldTypes::Timestamp:
            case FieldTypes::Year:
            case FieldTypes::String:
            case FieldTypes::VarString:
            case FieldTypes::Enum:
            case FieldTypes::Set:
            case FieldTypes::Bit:
            case FieldTypes::Blob:
            case FieldTypes::TinyBlob:
            case FieldTypes::MediumBlob:
            case FieldTypes::LongBlob:
            case FieldTypes::Geometry:
            case FieldTypes::Null:
                return fieldType;

            default:
                throw std::out_of_range{"Not a FieldType! Value == " + std::to_string(value)};
        }
    }

    namespace detail
    {
        /**
         * Conversion of field type to corresponding value of enum #enum_field_types.
         * As field types are defined though members of said enum, such conversion
         * cannot fail.
         */
        inline auto toMysqlEnum(FieldTypes fieldType)
        {
            return static_cast<enum_field_types>(fieldType);
        }
    }
}
