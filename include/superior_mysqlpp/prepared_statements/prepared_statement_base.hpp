/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <vector>
#include <sstream>

#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/types/optional.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/metadata.hpp>
#include <superior_mysqlpp/prepared_statements/validate_metadata_modes.hpp>
#include <superior_mysqlpp/prepared_statements/get_binding_type.hpp>
#include <superior_mysqlpp/types/nullable.hpp>

namespace SuperiorMySqlpp
{
    namespace detail
    {
        /**
         * Base internal class for prepared statements.
         * Thin wrapper over LowLevel::DBDriver::Statement.
         * Only subclasses are useful.
         */
        class StatementBase
        {
        protected:
            LowLevel::DBDriver::Statement statement;
            Loggers::ConstPointer_t loggerPtr;

        public:
            StatementBase(LowLevel::DBDriver::Statement&& statement, Loggers::ConstPointer_t loggerPtr=DefaultLogger::getLoggerPtr().get())
                : statement{std::move(statement)},
                  loggerPtr{loggerPtr}
            {
            }

            StatementBase(StatementBase&&) = default;
            StatementBase& operator=(StatementBase&&) = default;

            StatementBase(const StatementBase&) = delete;
            StatementBase& operator=(const StatementBase&) = delete;
        };

        /**
         * StoreOrUseResultBase is an extension of #StatementBase that adds respective
         * functionality specific to store() and use() mode of operation (see SuperiorMySqlpp::PreparedStatement
         * or SuperiorMySqlpp::Query for description of those) -- which one is
         * selected depends on storeResult template parameter.
         * @tparam storeResult Flag selecting between store() and use() specific specialization. Store is default one.
         *
         * Specialization of StatementBase where statement results are stored as a whole.
         * Contains additional methods related to whole result set like seeking specific row.
         */
        template<bool storeResult=true>
        class StoreOrUseResultBase : public StatementBase
        {
        public:
            using StatementBase::StatementBase;

            /**
             * Returns the number of rows in the result set.
             * @remark Note that this is designed for queries with results like SELECT,
             *         not for inserting or updating - method affectedRows() is designed for that
             *         (note - affectedRows() currently does not exist, addition is trivial).
             */
            auto getRowsCount()
            {
                return statement.getRowsCount();
            }

            /**
             * Seeks to an arbitrary row in a statement result set.
             * @param index Row number of seeked target.
             * @remark This function is templated, however single implementation
             *        with std::size_t would suffice.
             * EDIT: Reason why this function is templated is because private DBDriver::size_t is actually used.
             */
            template<typename T>
            void seekRow(T index)
            {
                statement.seekRow(index);
            }

            /**
             * Seeks to an arbitrary row in a statement result set using MYSQL_ROW_OFFSET.
             * Uses result of tellRowOffset, not a row number, so the access shall be faster.
             * @param offset Row offset obtained from tellRowOffset
             */
            void seekRowOffset(MYSQL_ROW_OFFSET offset)
            {
                statement.seekRowOffset(offset);
            }

            auto tellRowOffset()
            {
                return statement.tellRowOffset();
            }

        protected:
            /**
             * This method exist for both use() and store() specialization and implements the actual
             * difference in fetching strategy.
             * As this is specialization for store(), it calls appropriate method of C underlying client.
             */
            void storeOrUse()
            {
                this->statement.storeResult();
            }
        };

        /**
         * Specialization of #StoreOrUseResultBase where statement results are "used" - not
         * fetched all at one.
         * Contains methods that are useful for progressive fetching of results.
         */
        template<>
        class StoreOrUseResultBase<false> : public StatementBase
        {
        public:
            using StatementBase::StatementBase;

            /**
             * This method implements the use() specialized behavior.
             * As this is the default mode for mysql client, nothing needs to be done.
             */
            void storeOrUse() const
            {
            }

            /**
             * Returns the amount of fetched rows.
             * @return Amount of already fetched rows.
             * @remark Underlying function mysql_stmt_num_rows does NOT gurantee such behavior!
             *         According to its specification, it guarantees correct value only in store() mode.
             *         Experimentation suggests this truly does not work as intended (at least with MySql 5.5 client),
             *         removal or rewrite to use local counter is recommended.
             */
            auto getFetchedRowsCount()
            {
                return statement.getRowsCount();
            }
        };


        /**
         * Base class for static and dynamic prepared statements.
         * Contains mostly low level methods communicating directly with underlying mysql C library.
         * Also takes care for prepared statement's argument validation according to PreparedStatement settings.
         *
         * Settings itself are templated, so for different settings, different classes will be created.
         *
         * @tparam storeResult    flag - when true, mysql store result is used
         * @tparam validateMode   enum - says how params or results will be validated (more in {@link ValidateMetadataMode}).
         *                        When params or results doesn't comply with rules set by this option, exception will
         *                        be thrown.
         * @tparam warnMode       enum - says when user will be warned. These is same enum as #validateMode.
         *                        When params or results doesn't comply with rules set by this option a debug message
         *                        will be issued with description, which rule was broken.
         * @tparam ignoreNullable flag - says if {@link #validateResultMetadata()} ignores null value.
         *                        (Feasible only for prepare statement results.)
         *                        In mysql every data type can be also set to null. In C++ this is not the case.
         *                        At least not for primitive types. Usually when we read data from mysql, we also need
         *                        to set some "null" flag. In this library we can easily do it by mapping mysql data
         *                        type to Nullable<T> type, which is simple wrapper (for any data type), that can store
         *                        this "null flag". During result validation, this is also one of the checks - if we are
         *                        able to store this "null flag". Sometimes however we just want to ignore null flag
         *                        (binded result memory then will be not set, where null was sent), that is what this flag
         *                        is for.
         */
        template<bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable>
        class PreparedStatementBase : public StoreOrUseResultBase<storeResult>//, public ValidateResult<validateMetadataMode>
        {
        protected:
            /**
             * Collection of {@link detail::NullableBase Nullables} which needs to be engaged after
             * successful fetch. Must be filled by inherited class.
             */
            std::vector<detail::NullableBase*> nullableBindings;

        public:
            using StoreOrUseResultBase<storeResult>::StoreOrUseResultBase;

        private:
            Optional<ResultMetadata> resultMetadata;

            /**
             * This method is needed, because when SuperiorMySqlpp::Nullable type is created and
             * initialised using operator* in dynamic prepared statement,
             * its internal flag engaged is NOT set to true. This leads to
             * buggy behavior of isValid method in Nullable class.
             *
             * We need to call this in every fetch because only
             * nullables which are not null are engaged.
             */
            void engageNullables()
            {
                for(auto* nullable : nullableBindings) {
                    if (!nullable->isNull()) {
                        nullable->engage();
                    }
                }
            }

        public:
            /**
             * Returns metadata for the statement result.
             * There metadata are retrieveable even before execute().
             * @return Const reference to metadata.
             */
            const ResultMetadata& getResultMetadata()
            {
                return getModifiableResultMetadata();
            }

            /**
             * Returns modifiable metadata for the statement result.
             * There metadata are retrieveable even before execute().
             * @return Mutable reference to metadata.
             */
            ResultMetadata& getModifiableResultMetadata()
            {
                if (!resultMetadata)
                {
                    resultMetadata.emplace(std::move(this->statement.resultMetadata()));
                }
                return *resultMetadata;
            }

            auto fetchWithStatus()
            {
                auto status = this->statement.fetchWithStatus();

                if (status == LowLevel::DBDriver::Statement::FetchStatus::Ok) {
                    engageNullables();
                }

                return status;
            }

            /**
             * Perform fetch of next row of result set, returning status.
             * Truncation counts as unsuccessful.
             * @return Whether the fetch was successful or not.
             */
            auto fetch()
            {
                auto ok = this->statement.fetch();

                if (ok) {
                    engageNullables();
                }

                return ok;
            }

            bool fetchColumn()
            {
                this->statement.fetchColumn();
                engageNullables();
                return true;
            }

        public:
            /**
             * Allows sending data for long enough parameters in multiple chunks (through successive calls).
             * Parameter must have Text or Blob field type.
             * @param paramNumber Index of given parameter.
             * @param data C style string.
             * @param length Lenght of  data in bytes.
             * @return void
             * !!!!! Should return void, this form is misleading.
             */
            auto sendLongData(unsigned int paramNumber, const char* data, unsigned long length)
            {
                return this->statement.sendLongData(paramNumber, data, length);
            }

            /**
             * Allows sending data for long enough parameters in multiple chunks (through successive calls).
             * Parameter must have Text or Blob field type.
             * @param paramNumber Index of given parameter.
             * @param data String containing the new data.
             * @param length Lenght of data in bytes.
             * @return void
             * !!!!! Should return void, this form is misleading.
             */
            auto sendLongData(unsigned int paramNumber, const std::string& data)
            {
                return this->statement.sendLongData(paramNumber, data);
            }

            /**
             * Sets how many rows shall be prefetched in single fetch call.
             * @param count Amount of rows to be prefetched in single fetch. Is of type
             *              unsigned long, matching the required type in C MySQL client.
             *              Default value (if not set otherwise) is 1.
             * @remark This function does seem to only have purpose in use() subclass (see #StoreOrUseResultBase),
             *         consider moving it in the future.
             *         Also, according to MySql 5.7 documentation, it requires using cursor to take effect.
             *         However, at this time we are not exposing such functionality.
             */
            void setPrefetchRowCount(unsigned long count)
            {
                this->statement.setAttribute(LowLevel::DBDriver::Statement::Attributes::prefetchRows, count);
            }

            /**
             * Returns how many rows shall be prefetched in single fetch call.
             */
            unsigned long getPrefetchRowCount()
            {
                unsigned long result;
                this->statement.getAttribute(LowLevel::DBDriver::Statement::Attributes::prefetchRows, result);
                return result;
            }

            /**
             * If related attribute is set to true, the metadata for result field's maximum length is updated on store().
             * Field's maximum length is the length in bytes of the longest field value for the rows actually in the result set.
             * @remark For both kinds of prepared statements, store() is part of their execute() method.
             */
            void setUpdateMaxLengthOnStore(my_bool value)
            {
                this->statement.setAttribute(LowLevel::DBDriver::Statement::Attributes::updateMaxLength, value);
            }

            /**
             * Returns value of MySql attribute controling whether the metadata for field's metadata is updated on store.
             */
            bool getUpdateMaxLengthOnStore()
            {
                my_bool result = 0;
                this->statement.getAttribute(LowLevel::DBDriver::Statement::Attributes::updateMaxLength, result);
                return result;
            }


        public:
            /**
             * Validates metadata for given ResultBindings compared to metadata recieved from the server.
             * @remark Validation is performend after the statement is executed, but before the fetches.
             *         For #PreparedStatement, it happens directly in PreparedStatement::execute() method/
             *         For #DynamicPreparedStatement, it happens in DynamicPreparedStatement::updateResultBindings(),
             *             as the buffers may be (re)bound after execution.
             * @param resultBindings Bindings of type ResultBindings.
             */
            template<typename ResultBindings>
            void validateResultMetadata(const ResultBindings& resultBindings)
            {
                if (validateMode==ValidateMetadataMode::Disabled && warnMode==ValidateMetadataMode::Disabled)
                {
                    return;
                }

                auto&& metadata = this->getResultMetadata();

                std::size_t index = 0;
                auto bindingsIt=std::begin(resultBindings);
                auto bindingsEndIt=std::end(resultBindings);
                auto metadataIt=std::begin(metadata);
                auto metadataEndIt=std::end(metadata);
                while (bindingsIt!=bindingsEndIt && metadataIt!=metadataEndIt)
                {
                    auto&& binding = *bindingsIt;
                    auto&& resultMetadata = *metadataIt;

                    // It's needed to check buffer and length pointers to detect uninitialized binds.
                    // Buffer may point to nullptr in case bind is initialized as ArrayBase of zero length
                    // but the `length` will point to variable with buffer length in such case.
                    if (bindingsIt->buffer == nullptr && bindingsIt->length == nullptr)
                    {
                        throw PreparedStatementBindError("Uninitialized bind for result at index " + std::to_string(index) + "!");
                    }

                    auto&& bindingType = toFieldType(binding.buffer_type);
                    bool bindingIsUnsigned = binding.is_unsigned;
                    auto&& resultType = resultMetadata.getFieldType();
                    bool resultIsUnsigned = resultMetadata.isUnsigned();

                    if (bindingType == FieldTypes::Null)
                    {
                        continue;
                    }

                    // if compared types are not compatible within constraints of given validation level, raise exception
                    if (!isCompatible<validateMode>(resultType, resultIsUnsigned, bindingType, bindingIsUnsigned))
                    {
                        throw PreparedStatementTypeError{"Result types at index " + std::to_string(index) + " don't match!\n"
                                                         "In validate mode " + getValidateMetadataModeName(validateMode) + ":\n"
                                                         "expected type =" + detail::getBindingTypeFullName(bindingType, bindingIsUnsigned) +
                                                         "= is not compatible with "
                                                         "result type =" + detail::getBindingTypeFullName(resultType, resultIsUnsigned) + "="};
                    }

                    // if compared types are still not compatible within constraints of given warning level, log a warning
                    if (!isCompatible<warnMode>(resultType, resultIsUnsigned, bindingType, bindingIsUnsigned))
                    {
                        auto ptr = this->loggerPtr;
                        ptr->logMySqlStmtMetadataWarning(
                            this->statement.getDriverId(),
                            this->statement.getId(),
                            index,
                            warnMode,
                            bindingType,
                            bindingIsUnsigned,
                            resultType,
                            resultIsUnsigned
                        );
                    }

                    // We usually don't want to read nullable value into non-nullable one
                    // However it is possible, because mysql doesn't need to write this flag, to user provided place
                    // (see in mysql_stmt_bind_result() definition in file "libmysql.c".
                    // In this case however the result value is not set. We only ignore possible null value and we do
                    // it only when we explicitly want to.
                    if (resultMetadata.isNullable() && binding.is_null == nullptr && !ignoreNullable)
                    {
                        if (validateMode != ValidateMetadataMode::Disabled)
                        {
                            throw PreparedStatementTypeError{"Result types at index " + std::to_string(index) + " don't match!\n"
                                                             "You can't read nullable value into non-nullable one!!!"};
                        }
                        else
                        {
                            this->loggerPtr->logMySqlStmtMetadataNullableToNonNullableWarning(this->statement.getDriverId(), this->statement.getId(), index);
                        }
                    }

                    // next step
                    ++bindingsIt;
                    ++metadataIt;
                    ++index;
                }

                auto throwError = [](auto bindingsSize, auto metadataSize, auto&& excessiveTypes){
                    std::stringstream message{};
                    message << "Result bindings count (" << bindingsSize
                            << ") doesn't match number of returned fields (" << metadataSize <<  ")!"
                            << "Excessive types: ";
                    for (auto&& type: excessiveTypes)
                    {
                        message << detail::getBindingTypeFullName(std::get<0>(type), std::get<1>(type)) << ", ";
                    }
                    throw PreparedStatementTypeError(message.str());
                };

                // not all bindings were processed
                if (bindingsIt != bindingsEndIt)
                {
                    auto distance = std::distance(bindingsIt, bindingsEndIt);
                    auto bindingsSize = index + distance;
                    auto metadataSize = index;
                    std::vector<std::tuple<FieldTypes, bool>> excessiveTypes{};
                    excessiveTypes.reserve(distance);
                    do
                    {
                        excessiveTypes.emplace_back(toFieldType(bindingsIt->buffer_type), bindingsIt->is_unsigned);
                        ++bindingsIt;
                    } while (bindingsIt!=bindingsEndIt);
                    throwError(bindingsSize, metadataSize, excessiveTypes);
                }
                // not all metadata were processed
                else if (metadataIt != metadataEndIt)
                {
                    auto distance = std::distance(metadataIt, metadataEndIt);
                    auto bindingsSize = index;
                    auto metadataSize = index + distance;
                    std::vector<std::tuple<FieldTypes, bool>> excessiveTypes{};
                    excessiveTypes.reserve(distance);
                    do
                    {
                        excessiveTypes.emplace_back(metadataIt->getFieldType(), metadataIt->isUnsigned());
                        ++metadataIt;
                    } while (metadataIt != metadataEndIt);
                    throwError(bindingsSize, metadataSize, excessiveTypes);
                }
            }

        public:
            /**
             * For safety reasons, taking LowLevel::DBDriver::Statement reference from rvalue is prohibited.
             */
            LowLevel::DBDriver::Statement& detail_getStatementRef() && = delete;

            /**
             * Returns reference to encapsulated LowLevel::DBDriver's Statement.
             */
            LowLevel::DBDriver::Statement& detail_getStatementRef() &
            {
                return this->statement;
            }
        };
    }
}

