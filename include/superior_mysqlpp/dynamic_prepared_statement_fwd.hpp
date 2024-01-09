/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <superior_mysqlpp/prepared_statements/validate_metadata_modes.hpp>
#include <superior_mysqlpp/prepared_statement_fwd.hpp>


namespace SuperiorMySqlpp
{
    template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
             ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
             ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
             bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable()>
    class DynamicPreparedStatement;
}
