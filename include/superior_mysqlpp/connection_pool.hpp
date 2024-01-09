/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <utility>
#include <memory>

#include <superior_mysqlpp/shared_ptr_pool.hpp>
#include <superior_mysqlpp/connection.hpp>


namespace SuperiorMySqlpp
{
    class ConnectionHealthCheck
    {
    public:
        ConnectionHealthCheck() = default;
        ConnectionHealthCheck(const ConnectionHealthCheck&) = default;
        ConnectionHealthCheck(ConnectionHealthCheck&&) = default;

        ConnectionHealthCheck& operator=(const ConnectionHealthCheck&) = default;
        ConnectionHealthCheck& operator=(ConnectionHealthCheck&&) = default;

        template<typename Connection>
        auto operator()(Connection& connection) const
        {
            return std::async(std::launch::async, [&](){ return connection.tryPing(); });
        }
    };



    template<
        typename SharedPtrFactory,
        bool invalidateResourceOnAccess=false,
        /* Resource count keeper */
        bool enableResourceCountKeeper=true,
        bool terminateOnResourceCountKeeperFailure=false,
        /* Health care job */
        bool enableHealthCareJob=true,
        bool terminateOnHealthCareJobFailure=false,
        /* Pool management */
        template<typename> class PoolManagement=detail::NonPoolManagement
    >
    using ConnectionPool = SharedPtrPool<
            SharedPtrFactory,
            ConnectionHealthCheck,
            invalidateResourceOnAccess,
            /* Resource count keeper */
            enableResourceCountKeeper,
            terminateOnResourceCountKeeperFailure,
            /* Health care job */
            enableHealthCareJob,
            terminateOnHealthCareJobFailure,
            /* Pool management */
            PoolManagement
    >;


    template<
        bool invalidateResourceOnAccess=false,
        /* Resource count keeper */
        bool enableResourceCountKeeper=true,
        bool terminateOnResourceCountKeeperFailure=false,
        /* Health care job */
        bool enableHealthCareJob=true,
        bool terminateOnHealthCareJobFailure=false,
        /* Pool management */
        template<typename> class PoolManagement=detail::NonPoolManagement,
        typename SharedPtrFactory,
        typename LoggerPtr=decltype(DefaultLogger::getLoggerPtr())
    >
    auto makeConnectionPool(SharedPtrFactory&& factory, LoggerPtr&& loggerPtr=DefaultLogger::getLoggerPtr())
    {
        return ConnectionPool<
            std::decay_t<SharedPtrFactory>,
            invalidateResourceOnAccess,
            /* Resource count keeper */
            enableResourceCountKeeper,
            terminateOnResourceCountKeeperFailure,
            /* Health care job */
            enableHealthCareJob,
            terminateOnHealthCareJobFailure,
            /* Pool management */
            PoolManagement>
        {
            std::forward_as_tuple(std::forward<SharedPtrFactory>(factory)),
            std::forward_as_tuple(),
            std::forward_as_tuple(),
            std::forward<LoggerPtr>(loggerPtr)
        };
    }


}
