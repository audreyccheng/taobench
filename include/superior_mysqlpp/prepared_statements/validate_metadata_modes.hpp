/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <stdexcept>

#include <superior_mysqlpp/field_types.hpp>


namespace SuperiorMySqlpp
{
    /**
     * Enumeration of different strategies how to validate legality of field type conversions.
     */
    enum class ValidateMetadataMode
    {
        Disabled = 0, ///< No validation is performed
        Strict, ///< Only the exactly same data type is allowed
        Same, ///< Conversions between types with same internal representation allowed
        ArithmeticPromotions, ///< Conversions allowed between types preforming arithmetic promotions (converting to bigger type, like Short to Long)
        ArithmeticConversions, ///< Arbitary arithmentic conversions across numeric types allowed
    };

    /**
     * Function returning stringified ValidateMetadataMode enum.
     */
    inline const char* getValidateMetadataModeName(ValidateMetadataMode validateMetadataMode)
    {
        switch (validateMetadataMode)
        {
            case ValidateMetadataMode::Disabled:
                return "Disabled";

            case ValidateMetadataMode::Strict:
                return "Strict";

            case ValidateMetadataMode::Same:
                return "Same";

            case ValidateMetadataMode::ArithmeticPromotions:
                return "ArithmeticPromotions";

            case ValidateMetadataMode::ArithmeticConversions:
                return "ArithmeticConversions";

            default:
                throw std::logic_error{"The Universe is falling apart!"};
        }
    }

    /**
     * Function for given field type returns "pseudo-typeid" shared by same types.
     * @param type SuperiorMySqlpp::FieldTypes enumeration.
     * @return Arbitary integer that is however unique for each group of same types
     */
    inline int getSameTypeId(FieldTypes type)
    {
        switch (type)
        {
            case FieldTypes::Short:
            case FieldTypes::Year:
                return 0;

            case FieldTypes::Int24:
            case FieldTypes::Long:
                return 1;

            case FieldTypes::Datetime:
            case FieldTypes::Timestamp:
                return 2;

            case FieldTypes::Enum:
                return 3;

            case FieldTypes::Set:
                return 4;

            case FieldTypes::TinyBlob:
            case FieldTypes::MediumBlob:
            case FieldTypes::LongBlob:
            case FieldTypes::Blob:
            case FieldTypes::VarString:
            case FieldTypes::String:
                return 5;

            case FieldTypes::Geometry:
                return 11;

            case FieldTypes::Decimal:
                return 12;

            case FieldTypes::Tiny:
                return 13;

            case FieldTypes::LongLong:
                return 14;

            case FieldTypes::Float:
                return 15;

            case FieldTypes::Double:
                return 16;

            case FieldTypes::NewDecimal:
                return 17;

            case FieldTypes::Time:
                return 18;

            case FieldTypes::Date:
                return 19;

            case FieldTypes::Bit:
                return 20;

            // Not going to be used - to have switch complete only
            case FieldTypes::Null:
                return 21;
        }

        throw std::logic_error{"Universe is falling apart"};
    }

    /**
     * Returns true if field type uses is_unsigned field
     */
    inline bool fieldTypeHasSign(FieldTypes type)
    {
        switch (type)
        {
            case FieldTypes::Tiny:
            case FieldTypes::Short:
            case FieldTypes::Int24:
            case FieldTypes::Long:
            case FieldTypes::LongLong:
                return true;

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
            default:
                return false;
        }
    }

    /**
     * Returns true, if both types matches their signedness or given type has no signedness
     */
    inline bool fieldSignsMatch(FieldTypes from, bool from_is_unsigned, FieldTypes to, bool to_is_unsigned)
    {
        return from_is_unsigned == to_is_unsigned || !(fieldTypeHasSign(from) || fieldTypeHasSign(to));
    }

    /**
     * @brief Tests whether one argument is convertible to the other
     * Implementation depends on selected validation strategy
     * @params from Input field type.
     * @params from_is_unsigned Flag indicating signedness of input.
     * @params to Output field type.
     * @params to_is_unsigned Flag indicating signedness of output.
     * @return true if type of parameter from is compatible with parameter to.
     */

    template<ValidateMetadataMode>
    inline bool isCompatible(FieldTypes, bool, FieldTypes, bool) = delete;

    template<>
    inline bool isCompatible<ValidateMetadataMode::Disabled>(FieldTypes, bool, FieldTypes, bool)
    {
        return true;
    }

    template<>
    inline bool isCompatible<ValidateMetadataMode::Strict>(FieldTypes from, bool from_is_unsigned, FieldTypes to, bool to_is_unsigned)
    {
        return from==to && fieldSignsMatch(from, from_is_unsigned, to, to_is_unsigned);
    }

    template<>
    inline bool isCompatible<ValidateMetadataMode::Same>(FieldTypes from, bool from_is_unsigned, FieldTypes to, bool to_is_unsigned)
    {
        if (!fieldSignsMatch(from, from_is_unsigned, to, to_is_unsigned))
        {
            return false;
        }

        if (from == to)
        {
            return true;
        }
        else
        {
            return getSameTypeId(from) == getSameTypeId(to);
        }
    }

    template<>
    inline bool isCompatible<ValidateMetadataMode::ArithmeticPromotions>(FieldTypes from, bool from_is_unsigned, FieldTypes to, bool to_is_unsigned)
    {
        if (!from_is_unsigned && to_is_unsigned && fieldTypeHasSign(from) && fieldTypeHasSign(to))
        {
            return false;
        }

        switch (from)
        {
            // signed char
            case FieldTypes::Tiny:
                switch (to)
                {
                    case FieldTypes::Tiny:
                        return from_is_unsigned == to_is_unsigned;

                    case FieldTypes::Short:
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                    case FieldTypes::LongLong:
                    case FieldTypes::Float:
                    case FieldTypes::Double:
                        return true;

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
                    default:
                        return false;
                }

            // short int
            case FieldTypes::Short:
                switch (to)
                {
                    case FieldTypes::Short:
                        return from_is_unsigned == to_is_unsigned;

                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                    case FieldTypes::LongLong:
                    case FieldTypes::Float:
                    case FieldTypes::Double:
                        return true;

                    case FieldTypes::Tiny:
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
                    default:
                        return false;
                }


            // int
            case FieldTypes::Int24:
            case FieldTypes::Long:
                switch (to)
                {
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                        return from_is_unsigned == to_is_unsigned;

                    case FieldTypes::LongLong:
                    case FieldTypes::Double:
                        return true;

                    case FieldTypes::Tiny:
                    case FieldTypes::Short:
                    case FieldTypes::Float:
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
                    default:
                        return false;
                }


            // long long int
            case FieldTypes::LongLong:
                switch (to)
                {
                    case FieldTypes::LongLong:
                        return from_is_unsigned == to_is_unsigned;

                    case FieldTypes::Tiny:
                    case FieldTypes::Short:
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
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
                    default:
                        return false;
                }

            // float
            case FieldTypes::Float:
                switch (to)
                {
                    case FieldTypes::Float:
                    case FieldTypes::Double:
                        return true;

                    case FieldTypes::Tiny:
                    case FieldTypes::Short:
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                    case FieldTypes::LongLong:
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
                    default:
                        return false;
                }

            // double
            case FieldTypes::Double:
                switch (to)
                {
                    case FieldTypes::Double:
                        return true;

                    case FieldTypes::Tiny:
                    case FieldTypes::Short:
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                    case FieldTypes::LongLong:
                    case FieldTypes::Float:
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
                    default:
                        return false;
                }

            case FieldTypes::Decimal:
            case FieldTypes::NewDecimal:
            case FieldTypes::Time:
            case FieldTypes::Date:
            case FieldTypes::Year:
            case FieldTypes::Datetime:
            case FieldTypes::Timestamp:
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
            default:
                // TODO: it could be done more efficiently by another switch
                return isCompatible<ValidateMetadataMode::Same>(from, from_is_unsigned, to, to_is_unsigned);
        }
    }

    template<>
    inline bool isCompatible<ValidateMetadataMode::ArithmeticConversions>(FieldTypes from, bool from_is_unsigned, FieldTypes to, bool to_is_unsigned)
    {
        switch (from)
        {
            case FieldTypes::Tiny:
            case FieldTypes::Short:
            case FieldTypes::Int24:
            case FieldTypes::Long:
            case FieldTypes::LongLong:
            case FieldTypes::Float:
            case FieldTypes::Double:
                switch (to)
                {
                    case FieldTypes::Tiny:
                    case FieldTypes::Short:
                    case FieldTypes::Int24:
                    case FieldTypes::Long:
                    case FieldTypes::LongLong:
                    case FieldTypes::Float:
                    case FieldTypes::Double:
                        return true;

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
                    default:
                        return false;
                }

            case FieldTypes::Decimal:
            case FieldTypes::NewDecimal:
            case FieldTypes::Time:
            case FieldTypes::Date:
            case FieldTypes::Year:
            case FieldTypes::Datetime:
            case FieldTypes::Timestamp:
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
            default:
                // TODO: it could be done more efficiently by another switch
                return isCompatible<ValidateMetadataMode::Same>(from, from_is_unsigned, to, to_is_unsigned);
        }
    }
}
