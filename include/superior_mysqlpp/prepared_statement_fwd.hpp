/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <superior_mysqlpp/prepared_statements/validate_metadata_modes.hpp>

/*
 * This file contains only forward declarations for PreparedStatement related types,
 * for more information see their proper definitions.
 */

namespace SuperiorMySqlpp
{
    template<bool IsParamBinding, typename... Types>
    class Bindings;

    template<typename... Types>
    using ParamBindings = Bindings<true, Types...>;

    template<typename... Types>
    using ResultBindings = Bindings<false, Types...>;


    namespace detail
    {
        namespace PreparedStatementsDefault
        {
            constexpr auto getStoreResult()
            {
                return true;
            }

            constexpr auto getValidateMode()
            {
                return ValidateMetadataMode::ArithmeticPromotions;
            }

            constexpr auto getWarnMode()
            {
                return ValidateMetadataMode::Same;
            }

            /**
             * Returns default value for prepared statement flag ignoreNullable.
             * Usually nullable should not be ignored.
             * @return always false
             */
            constexpr auto getIgnoreNullable()
            {
                return false;
            }
        }
    }

    /*
     * Forward declaration of PreparedStatement.
     * Declares defaults for template parameters.
     */
    template<typename ResultBindings, typename ParamBindings,
             bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable()>
    class PreparedStatement;
}

