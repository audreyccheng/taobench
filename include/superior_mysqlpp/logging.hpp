/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <cinttypes>
#include <atomic>
#include <mutex>
#include <utility>

#include <superior_mysqlpp/types/string_view.hpp>
#include <superior_mysqlpp/types/spin_guard.hpp>
#include <superior_mysqlpp/prepared_statements/validate_metadata_modes.hpp>
#include <superior_mysqlpp/prepared_statements/get_binding_type.hpp>
#include <superior_mysqlpp/utils.hpp>


namespace SuperiorMySqlpp {

    namespace detail {

        /**
         * Helper function printing its arguments into std::cerr via operator <<
         * @remark Parameters are perfect forwarded and are followed by implicit std::endl.
         * @param lock Reference to atomic lock used to gate the output
         * @param args Variable number of arguments of any type
         */
        template <typename... Args>
        void logStderr(std::atomic_flag &lock, Args&&... args) {
            SuperiorMySqlpp::SpinGuard guard{lock};
            (void) guard; // Unused variable otherwise
            streamify(std::cerr, std::forward<Args>(args)...) << std::endl;
        }

    }

    #define __deprecated_logger_method \
        [[deprecated("This method is deprecated in favor of method with hostname argument")]]

    namespace Loggers
    {
        /*
         * All logging functions are noexcept since it is user's
         * responsibility to deal with errors (exceptions) when logging.
         */

        /*
         * You are to choose to derive either from class Interface or class Base
         * so you shall either implement all logging functions or only those you want.
         */

        class Interface
        {
        public:
            Interface() = default;
            Interface(const Interface&) = default;
            Interface(Interface&&) = default;
            Interface& operator=(const Interface&) = default;
            Interface& operator=(Interface&&) = default;
            virtual ~Interface() = default;

            virtual void logSharedPtrPoolEmergencyResourceCreation(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolErasingResource(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept = 0;
            virtual void logSharedPtrPoolClearPool(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolEmergencyResourceAdded(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolEmergencyResourceAdditionSkippedForNewPopulation(std::uint_fast64_t /*poolId*/) const noexcept = 0;

            virtual void logSharedPtrPoolResourceCountKeeperCycleStart(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperStoped(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperTooLittleResources(
                std::uint_fast64_t /*poolId*/,
                std::size_t /*availableCount*/,
                std::size_t /*needed*/,
                std::size_t /*used*/,
                std::size_t /*size*/
            ) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperStateOK(
                std::uint_fast64_t /*poolId*/,
                std::size_t /*availableCount*/,
                std::size_t /*used*/,
                std::size_t /*size*/
            ) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperAddedResources(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperAdditionSkippedForNewPopulation(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t /*poolId*/, std::size_t /*count*/, const std::exception&) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperTooManyResources(std::uint_fast64_t /*poolId*/, std::size_t /*availableCount*/, std::size_t /*removeCount*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperDisposingResources(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept = 0;
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t /*poolId*/) const noexcept = 0;

            virtual void logSharedPtrPoolHealthCareJobCycleStart(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobLockedPtr(std::uint_fast64_t /*poolId*/, void* /*availableCount*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobUnableToLockPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobLockedSize(std::uint_fast64_t /*poolId*/, std::size_t /*size*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobHealthCheckForPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobErasingPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobLeavingHealthyResource(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobHealthCheckCompleted(std::uint_fast64_t /*poolId*/, std::size_t /*completed*/, std::size_t /*locked*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobHealthCheckFinished(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobCycleFinished(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobStopped(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept = 0;
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t /*poolId*/) const noexcept = 0;

            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t /*poolId*/) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t /*poolId*/)const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept = 0;
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/) const noexcept = 0;


            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, std::exception&, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t /*poolId*/, const std::string &)const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, std::exception&, const std::string &) const noexcept = 0;
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept = 0;

            virtual void logMySqlConnecting(std::uint_fast64_t /*connectionId*/, const char* /*host*/, const char* /*user*/,
                const char* /*database*/, std::uint16_t /*port*/, const char* /*socketPath*/) const noexcept = 0;
            virtual void logMySqlConnected(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlClose(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlPing(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlQuery(std::uint_fast64_t /*connectionId*/, const StringView& /*query*/) const noexcept = 0;
            virtual void logMySqlStartTransaction(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlCommitTransaction(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlRollbackTransaction(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
            virtual void logMySqlStmtPrepare(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, const StringView& /*query*/) const noexcept = 0;
            virtual void logMySqlStmtExecute(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/) const noexcept = 0;
            virtual void logMySqlStmtMetadataWarning(
                    std::uint_fast64_t /*connectionId*/,
                    std::uint_fast64_t /*id*/,
                    std::size_t /*index*/,
                    ValidateMetadataMode /*warnMode*/,
                    FieldTypes /*bindingType*/,
                    bool /*isBindingUnsigned*/,
                    FieldTypes /*resultType*/,
                    bool /*isResultUnsigned*/
            ) const noexcept = 0;
            virtual void logMySqlStmtMetadataNullableToNonNullableWarning(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, std::size_t /*index*/) const noexcept = 0;

            virtual void logMySqlStmtClose(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/) const noexcept = 0;
            virtual void logMySqlStmtCloseError(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, const StringView& /*mysqlError*/) const noexcept = 0;
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t /*connectionId*/, std::exception&) const noexcept = 0;
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t /*connectionId*/) const noexcept = 0;
        };


        class Base : public Interface
        {
        public:
            Base() = default;
            Base(const Base&) = default;
            Base(Base&&) = default;
            Base& operator=(const Base&) = default;
            Base& operator=(Base&&) = default;
            virtual ~Base() = default;

            virtual void logSharedPtrPoolEmergencyResourceCreation(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolErasingResource(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept override {}
            virtual void logSharedPtrPoolClearPool(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolEmergencyResourceAdded(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolEmergencyResourceAdditionSkippedForNewPopulation(std::uint_fast64_t /*poolId*/) const noexcept override {}

            virtual void logSharedPtrPoolResourceCountKeeperCycleStart(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperStoped(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperTooLittleResources(
                std::uint_fast64_t /*poolId*/,
                std::size_t /*availableCount*/,
                std::size_t /*needed*/,
                std::size_t /*used*/,
                std::size_t /*size*/
            ) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperStateOK(
                std::uint_fast64_t /*poolId*/,
                std::size_t /*availableCount*/,
                std::size_t /*used*/,
                std::size_t /*size*/
            ) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperAddedResources(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperAdditionSkippedForNewPopulation(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t /*poolId*/, std::size_t /*count*/, const std::exception&) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperTooManyResources(std::uint_fast64_t /*poolId*/, std::size_t /*availableCount*/, std::size_t /*removeCount*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperDisposingResources(std::uint_fast64_t /*poolId*/, std::size_t /*count*/) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept override {}
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t /*poolId*/) const noexcept override {}

            virtual void logSharedPtrPoolHealthCareJobCycleStart(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobLockedPtr(std::uint_fast64_t /*poolId*/, void* /*availableCount*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobUnableToLockPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobLockedSize(std::uint_fast64_t /*poolId*/, std::size_t /*size*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobHealthCheckForPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobErasingPtr(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobLeavingHealthyResource(std::uint_fast64_t /*poolId*/, void* /*resourceAddress*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobHealthCheckCompleted(std::uint_fast64_t /*poolId*/, std::size_t /*completed*/, std::size_t /*locked*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobHealthCheckFinished(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobCycleFinished(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobStopped(std::uint_fast64_t /*poolId*/) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t /*poolId*/, const std::exception&) const noexcept override {}
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t /*poolId*/) const noexcept override {}

            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t /*poolId*/)const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/) const noexcept override {}

            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, std::exception&, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, std::exception&, const std::string &) const noexcept override {}
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, const std::string &) const noexcept override {}

            virtual void logMySqlConnecting(std::uint_fast64_t /*connectionId*/, const char* /*host*/, const char* /*user*/,
                const char* /*database*/, std::uint16_t /*port*/, const char* /*socketPath*/) const noexcept override {}
            virtual void logMySqlConnected(std::uint_fast64_t /*connectionId*/) const noexcept override {}
            virtual void logMySqlClose(std::uint_fast64_t /*connectionId*/) const noexcept override {}
            virtual void logMySqlPing(std::uint_fast64_t /*connectionId*/) const noexcept override {}
            virtual void logMySqlQuery(std::uint_fast64_t /*connectionId*/, const StringView& /*query*/) const noexcept override {}
            virtual void logMySqlStartTransaction(std::uint_fast64_t /*connectionId*/) const noexcept override {};
            virtual void logMySqlCommitTransaction(std::uint_fast64_t /*connectionId*/) const noexcept override {};
            virtual void logMySqlRollbackTransaction(std::uint_fast64_t /*connectionId*/) const noexcept override {};
            virtual void logMySqlStmtPrepare(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, const StringView& /*query*/) const noexcept override {}
            virtual void logMySqlStmtExecute(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/) const noexcept override {}
            virtual void logMySqlStmtMetadataWarning(
                    std::uint_fast64_t /*connectionId*/,
                    std::uint_fast64_t /*id*/,
                    std::size_t /*index*/,
                    ValidateMetadataMode /*warnMode*/,
                    FieldTypes /*bindingType*/,
                    bool /*isBindingUnsigned*/,
                    FieldTypes /*resultType*/,
                    bool /*isResultUnsigned*/
            ) const noexcept override {}
            virtual void logMySqlStmtMetadataNullableToNonNullableWarning(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, std::size_t /*index*/) const noexcept override {}

            virtual void logMySqlStmtClose(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/) const noexcept override {}
            virtual void logMySqlStmtCloseError(std::uint_fast64_t /*connectionId*/, std::uint_fast64_t /*id*/, const StringView& /*mysqlError*/) const noexcept override {}
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t /*connectionId*/, std::exception&) const noexcept override {}
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t /*connectionId*/) const noexcept override {}
        };


        class Full final : public Interface
        {
        private:
            mutable std::atomic_flag lock{};

        public:
            using Interface::Interface;
            virtual ~Full() override = default;


            virtual void logMySqlConnecting(
                    std::uint_fast64_t id,
                    const char* host,
                    const char* user,
                    const char* database,
                    std::uint16_t port,
                    const char* socketPath
                ) const noexcept override
            {
                detail::logStderr(lock,
                    "Connection [", id, "]: Connecting to \"", (host)? host : "", "\". {user=\"", (user)? user : "",
                    "\", database=\"", (database)? database : "", "\", socketPath=\"", (socketPath)? socketPath : "",
                    "\", port=", port, "}"
                );
            }

            virtual void logMySqlConnected(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Connection [", id, "]: Connected.");
            }

            virtual void logMySqlClose(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Connection [", id, "]: Closed.");
            }

            virtual void logMySqlPing(std::uint_fast64_t connectionId) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Ping.");
            }

            virtual void logMySqlQuery(std::uint_fast64_t connectionId, const StringView& query) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Query: \"", query, "\".");
            }

            virtual void logMySqlStartTransaction(std::uint_fast64_t connectionId) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Start transaction.");
            }

            virtual void logMySqlCommitTransaction(std::uint_fast64_t connectionId) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Commit transaction.");
            }

            virtual void logMySqlRollbackTransaction(std::uint_fast64_t connectionId) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Rollback transaction.");
            }

            virtual void logMySqlStmtPrepare(std::uint_fast64_t connectionId, std::uint_fast64_t id, const StringView& statement) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: PS [", id, "]: Prepare: \"", statement, "\".");
            }

            virtual void logMySqlStmtExecute(std::uint_fast64_t connectionId, std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: PS [", id, "]: Executing.");
            }

            static void logMySqlStmtMetadataWarningStatic(
                    std::atomic_flag &lock,
                    std::uint_fast64_t connectionId,
                    std::uint_fast64_t id,
                    std::size_t index,
                    ValidateMetadataMode warnMode,
                    FieldTypes bindingType,
                    bool isBindingUnsigned,
                    FieldTypes resultType,
                    bool isResultUnsigned
                ) noexcept
            {
                detail::logStderr(lock,
                    "Connection [", connectionId, "]: PS [", id, "]: ",
                    "Result types at index ", index, " don't match!\n",
                    "In warn mode ", getValidateMetadataModeName(warnMode), ":\n",
                    "expected type =", detail::getBindingTypeFullName(bindingType, isBindingUnsigned),
                    "= is not compatible with ",
                    "result type =", detail::getBindingTypeFullName(resultType, isResultUnsigned), "="
                );
            }
            virtual void logMySqlStmtMetadataWarning(
                    std::uint_fast64_t connectionId,
                    std::uint_fast64_t id,
                    std::size_t index,
                    ValidateMetadataMode warnMode,
                    FieldTypes bindingType,
                    bool isBindingUnsigned,
                    FieldTypes resultType,
                    bool isResultUnsigned
                ) const noexcept override
            {
                logMySqlStmtMetadataWarningStatic(
                    lock,
                    connectionId,
                    id,
                    index,
                    warnMode,
                    bindingType,
                    isBindingUnsigned,
                    resultType,
                    isResultUnsigned
                );
            }

            static void logMySqlStmtMetadataNullableToNonNullableWarningStatic(std::atomic_flag &lock, std::uint_fast64_t connectionId, std::uint_fast64_t id, std::size_t index) noexcept
            {
                detail::logStderr(lock,
                    "Connection [", connectionId, "]: PS [", id, "]: Result types at index ", index, " don't match!\n",
                    "You can't read nullable value into non-nullable one!!!");
            }
            virtual void logMySqlStmtMetadataNullableToNonNullableWarning(std::uint_fast64_t connectionId, std::uint_fast64_t id, std::size_t index) const noexcept override
            {
                logMySqlStmtMetadataNullableToNonNullableWarningStatic(lock, connectionId, id, index);
            }

            virtual void logMySqlStmtClose(std::uint_fast64_t connectionId, std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: PS [", id, "]: Closing.");
            }

            static void logMySqlStmtCloseErrorStatic(std::atomic_flag &lock, std::uint_fast64_t connectionId, std::uint_fast64_t id, const StringView& mysqlError) noexcept
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: PS [", id, "]: Closing statement failed with error:\n", mysqlError);
            }
            virtual void logMySqlStmtCloseError(std::uint_fast64_t connectionId, std::uint_fast64_t id, const StringView& mysqlError) const noexcept override
            {
                logMySqlStmtCloseErrorStatic(lock, connectionId, id, mysqlError);
            }

            static void logMySqlTransactionRollbackFailedStatic(std::atomic_flag &lock, std::uint_fast64_t connectionId, std::exception& e) noexcept
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Rollback failed with error:\n", e.what());
            }
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t connectionId, std::exception& e) const noexcept override
            {
                logMySqlTransactionRollbackFailedStatic(lock, connectionId, e);
            }

            static void logMySqlTransactionRollbackFailedStatic(std::atomic_flag &lock, std::uint_fast64_t connectionId) noexcept
            {
                detail::logStderr(lock, "Connection [", connectionId, "]: Rollback failed with unknown error!!!");
            }
            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t connectionId) const noexcept override
            {
                logMySqlTransactionRollbackFailedStatic(lock, connectionId);
            }


            virtual void logSharedPtrPoolEmergencyResourceCreation(std::uint_fast64_t poolId) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: Emergency resource creation.");
            }

            virtual void logSharedPtrPoolErasingResource(std::uint_fast64_t poolId, void* resourceAddress) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: Erasing resource at address: ", resourceAddress, ".");
            }

            virtual void logSharedPtrPoolClearPool(std::uint_fast64_t poolId) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: CLEARING POOL!!!");
            }

            virtual void logSharedPtrPoolEmergencyResourceAdded(std::uint_fast64_t poolId) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: Emergency resource added.");
            }

            virtual void logSharedPtrPoolEmergencyResourceAdditionSkippedForNewPopulation(std::uint_fast64_t poolId) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: Emergency resource addition has been skipped due to new population arising .");
            }

            virtual void logSharedPtrPoolResourceCountKeeperCycleStart(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper cycle start.");
            }

            virtual void logSharedPtrPoolResourceCountKeeperStoped(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper stopped.");
            }

            virtual void logSharedPtrPoolResourceCountKeeperTooLittleResources(
                    std::uint_fast64_t id, std::size_t availableCount, std::size_t needed, std::size_t used, std::size_t size
                ) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper too little resources. ",
                        "AvailableCount: ", availableCount, "; needed: ", needed, "; used: ", used, "; size: ", size,"");
            }

            virtual void logSharedPtrPoolResourceCountKeeperStateOK(
                    std::uint_fast64_t id, std::size_t availableCount, std::size_t used, std::size_t size
                ) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper state of resources OK. ",
                        "AvailableCount: ", availableCount, "; used: ", used, "; size: ", size,"");
            }

            virtual void logSharedPtrPoolResourceCountKeeperAddedResources(std::uint_fast64_t id, std::size_t count) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper added ", count, " resources.");
            }

            virtual void logSharedPtrPoolResourceCountKeeperAdditionSkippedForNewPopulation(std::uint_fast64_t poolId) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, "]: Resource count keeper - resource additions has been skipped due to new population arising .");
            }

            static void logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(std::atomic_flag &lock, std::uint_fast64_t id, std::size_t count, const std::exception& e) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper adding ", count, " resources ERROR:\n", e.what());
            }
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t id, std::size_t count, const std::exception& e) const noexcept override
            {
                logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(lock, id, count, e);
            }

            static void logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(std::atomic_flag &lock, std::uint_fast64_t id, std::size_t count) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper adding ", count, " resources unknown ERROR!");
            }
            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t id, std::size_t count) const noexcept override
            {
                logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(lock, id, count);
            }

            virtual void logSharedPtrPoolResourceCountKeeperTooManyResources(std::uint_fast64_t id, std::size_t availableCount, std::size_t needed) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper too many resources. ",
                        "AvailableCount: ", availableCount, "; needed: ", needed,"");
            }

            virtual void logSharedPtrPoolResourceCountKeeperDisposingResources(std::uint_fast64_t id, std::size_t count) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper disposing ", count, " resources.");
            }

            static void logSharedPtrPoolResourceCountKeeperErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id, const std::exception& e) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper ERROR:\n", e.what());
            }
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                logSharedPtrPoolResourceCountKeeperErrorStatic(lock, id, e);
            }

            static void logSharedPtrPoolResourceCountKeeperErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Resource count keeper unknown ERROR!");
            }
            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t id) const noexcept override
            {
                logSharedPtrPoolResourceCountKeeperErrorStatic(lock, id);
            }


            virtual void logSharedPtrPoolHealthCareJobCycleStart(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job cycle start.");
            }

            virtual void logSharedPtrPoolHealthCareJobLockedPtr(std::uint_fast64_t id, void* ptr) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job locked pointer: ", ptr);
            }

            virtual void logSharedPtrPoolHealthCareJobUnableToLockPtr(std::uint_fast64_t id, void* ptr) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job unable to lock pointer: ", ptr);
            }

            virtual void logSharedPtrPoolHealthCareJobLockedSize(std::uint_fast64_t id, std::size_t size) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job locked size: ", size);
            }

            virtual void logSharedPtrPoolHealthCareJobHealthCheckForPtr(std::uint_fast64_t id, void* ptr) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job running check for: ", ptr);
            }

            virtual void logSharedPtrPoolHealthCareJobErasingPtr(std::uint_fast64_t id, void* ptr) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job erasing resource: ", ptr);
            }

            virtual void logSharedPtrPoolHealthCareJobLeavingHealthyResource(std::uint_fast64_t id, void* ptr) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job leaving healthy resource: ", ptr);
            }

            virtual void logSharedPtrPoolHealthCareJobHealthCheckCompleted(std::uint_fast64_t id, std::size_t compled, std::size_t locked) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job health check completed for ", compled, " of ", locked, ".");
            }

            static void logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id, const std::exception& e) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job health check ERROR:\n", e.what());
            }
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(lock, id, e);
            }

            static void logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job health check unknown ERROR!");
            }
            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t id) const noexcept override
            {
                logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(lock, id);
            }

            virtual void logSharedPtrPoolHealthCareJobHealthCheckFinished(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job health check finished.");
            }

            virtual void logSharedPtrPoolHealthCareJobCycleFinished(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job cycle finished.");
            }

            virtual void logSharedPtrPoolHealthCareJobStopped(std::uint_fast64_t id) const noexcept override
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job stopped.");
            }

            static void logSharedPtrPoolHealthCareJobErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id, const std::exception& e) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job ERROR:\n", e.what());
            }
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                logSharedPtrPoolHealthCareJobErrorStatic(lock, id, e);
            }

            static void logSharedPtrPoolHealthCareJobErrorStatic(std::atomic_flag &lock, std::uint_fast64_t id) noexcept
            {
                detail::logStderr(lock, "Pool [", id, "]: Health care job unknown ERROR!");
            }
            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t id) const noexcept override
            {
                logSharedPtrPoolHealthCareJobErrorStatic(lock, id);
            }

            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t /*poolId*/) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t /*poolId*/)const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/, std::exception&) const noexcept override {}
            __deprecated_logger_method virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t /*poolId*/) const noexcept override {}

            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleStart(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Cycle start.");
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementChangeDetected(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Change detected.");
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementCycleEnd(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Cycle end.");
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementStopped(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Stopped.");
            }

            static void logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(std::atomic_flag &lock, std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) noexcept
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Check error:\n", e.what());
            }
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) const noexcept override
            {
                logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(lock, poolId, e, hostname);
            }

            static void logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(std::atomic_flag &lock, std::uint_fast64_t poolId, const std::string &hostname) noexcept
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Check unknown error!!!");
            }
            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(lock, poolId, hostname);
            }

            static void logSharedPtrPoolDnsAwarePoolManagementErrorStatic(std::atomic_flag &lock, std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) noexcept
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Error:\n", e.what());
            }
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) const noexcept override
            {
                logSharedPtrPoolDnsAwarePoolManagementErrorStatic(lock, poolId, e, hostname);
            }

            static void logSharedPtrPoolDnsAwarePoolManagementErrorStatic(std::atomic_flag &lock, std::uint_fast64_t poolId, const std::string &hostname) noexcept
            {
                detail::logStderr(lock, "Pool [", poolId, ", hostname=", hostname, "]: DnsAwarePoolManagement: Unknown error!!!");
            }
            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                logSharedPtrPoolDnsAwarePoolManagementErrorStatic(lock, poolId, hostname);
            }
        };


        class Default final : public Base
        {
        private:
            mutable std::atomic_flag lock{};
        public:
            using Base::Base;
            virtual ~Default() override = default;

            virtual void logMySqlStmtMetadataWarning(
                    std::uint_fast64_t connectionId,
                    std::uint_fast64_t id,
                    std::size_t index,
                    ValidateMetadataMode warnMode,
                    FieldTypes bindingType,
                    bool bindingIsUnsigned,
                    FieldTypes resultType,
                    bool resultIsUnsigned
                ) const noexcept override
            {
                Full::logMySqlStmtMetadataWarningStatic(
                    lock,
                    connectionId,
                    id,
                    index,
                    warnMode,
                    bindingType,
                    bindingIsUnsigned,
                    resultType,
                    resultIsUnsigned
                );
            }

            virtual void logMySqlStmtMetadataNullableToNonNullableWarning(
                    std::uint_fast64_t connectionId,
                    std::uint_fast64_t id,
                    std::size_t index
                ) const noexcept override
            {
                Full::logMySqlStmtMetadataNullableToNonNullableWarningStatic(lock, connectionId, id, index);
            }

            virtual void logMySqlStmtCloseError(std::uint_fast64_t connectionId, std::uint_fast64_t id, const StringView& mysqlError) const noexcept override
            {
                Full::logMySqlStmtCloseErrorStatic(lock, connectionId, id, mysqlError);
            }

            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t connectionId, std::exception& e) const noexcept override
            {
                Full::logMySqlTransactionRollbackFailedStatic(lock, connectionId, e);
            }

            virtual void logMySqlTransactionRollbackFailed(std::uint_fast64_t connectionId) const noexcept override
            {
                Full::logMySqlTransactionRollbackFailedStatic(lock, connectionId);
            }

            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t id, std::size_t count, const std::exception& e) const noexcept override
            {
                Full::logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(lock, id, count, e);
            }

            virtual void logSharedPtrPoolResourceCountKeeperAddingResourcesException(std::uint_fast64_t id, std::size_t count) const noexcept override
            {
                Full::logSharedPtrPoolResourceCountKeeperAddingResourcesExceptionStatic(lock, id, count);
            }

            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                Full::logSharedPtrPoolResourceCountKeeperErrorStatic(lock, id, e);
            }

            virtual void logSharedPtrPoolResourceCountKeeperError(std::uint_fast64_t id) const noexcept override
            {
                Full::logSharedPtrPoolResourceCountKeeperErrorStatic(lock, id);
            }

            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                Full::logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(lock, id, e);
            }

            virtual void logSharedPtrPoolHealthCareJobHealthCheckError(std::uint_fast64_t id) const noexcept override
            {
                Full::logSharedPtrPoolHealthCareJobHealthCheckErrorStatic(lock, id);
            }

            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t id, const std::exception& e) const noexcept override
            {
                Full::logSharedPtrPoolHealthCareJobErrorStatic(lock, id, e);
            }

            virtual void logSharedPtrPoolHealthCareJobError(std::uint_fast64_t id) const noexcept override
            {
                Full::logSharedPtrPoolHealthCareJobErrorStatic(lock, id);
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) const noexcept override
            {
                Full::logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(lock, poolId, e, hostname);
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementCheckError(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                Full::logSharedPtrPoolDnsAwarePoolManagementCheckErrorStatic(lock, poolId, hostname);
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t poolId, std::exception& e, const std::string &hostname) const noexcept override
            {
                Full::logSharedPtrPoolDnsAwarePoolManagementErrorStatic(lock, poolId, e, hostname);
            }

            virtual void logSharedPtrPoolDnsAwarePoolManagementError(std::uint_fast64_t poolId, const std::string &hostname) const noexcept override
            {
                Full::logSharedPtrPoolDnsAwarePoolManagementErrorStatic(lock, poolId, hostname);
            }
        };


        using Pointer_t = Interface*;
        using ConstPointer_t = const Interface*;
        using SharedPointer_t = std::shared_ptr<Loggers::Interface>;
    }

    #undef __deprecated_logger_method

    /*
     * Phoenix singleton
     */
    class DefaultLogger
    {
    public:
        using LoggetPtr_t = Loggers::SharedPointer_t;

    private:
        bool destroyed{false};
        LoggetPtr_t loggerPtr{std::make_shared<Loggers::Default>()};
        DefaultLogger() = default;

    private:
        DefaultLogger(const DefaultLogger&) = delete;
        DefaultLogger(DefaultLogger&&) = delete;
        DefaultLogger& operator=(const DefaultLogger&) = delete;
        DefaultLogger& operator=(DefaultLogger&&) = delete;

        ~DefaultLogger()
        {
            destroyed = true;
        }

        static auto& getInstanceUnsafe()
        {
            static DefaultLogger instance{};
            return instance;
        }

        static void destroy()
        {
            getInstanceUnsafe().~DefaultLogger();
        }

    public:
        [[deprecated("Typo in method name; Use isDestroyed() method instead.")]]
        static bool isDestoyed()
        {
            return isDestroyed();
        }

        static bool isDestroyed()
        {
            return getInstanceUnsafe().destroyed;
        }

        static auto& getModifiableInstance()
        {
            auto& instance = getInstanceUnsafe();

            // Double-checked locking pattern
            if (instance.destroyed)
            {
                /*
                 * Phoenix shall rise from the ashes!!!
                 * (Even with default configuration, it will prevent dead reference problem!)
                 */
                static std::atomic_flag spinFlag{false};
                SpinGuard guard{spinFlag};

                if (instance.destroyed)
                {
                    new(std::addressof(instance)) DefaultLogger{};
                    instance.destroyed = false;
                    std::atexit(&destroy);
                }
            }
            return instance;
        }

        static const auto& getInstance()
        {
            return getModifiableInstance();
        }

        static const auto& getLoggerPtr()
        {
            return getInstance().loggerPtr;
        }

        template<typename T>
        static void setLoggerPtr(T&& value)
        {
            getModifiableInstance().loggerPtr = std::forward<T>(value);
        }
    };
}
