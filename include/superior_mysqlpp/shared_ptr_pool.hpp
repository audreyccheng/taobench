/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <superior_mysqlpp/shared_ptr_pool/base.hpp>
#include <superior_mysqlpp/shared_ptr_pool/resource_count_keeper.hpp>
#include <superior_mysqlpp/shared_ptr_pool/health_care_job.hpp>


namespace SuperiorMySqlpp
{
    namespace detail
    {
        template<typename Base>
        struct NonPoolManagement
        {};
    }


template<
    /* Basic pool */
    typename SharedPtrFactory,
    typename ResourceHealthCheck=void,
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
#define THIS_T SharedPtrPool<SharedPtrFactory, ResourceHealthCheck, invalidateResourceOnAccess, enableResourceCountKeeper, terminateOnResourceCountKeeperFailure, enableHealthCareJob, terminateOnHealthCareJobFailure, PoolManagement>
class SharedPtrPool :
    /* Basic pool */
    public SharedPtrPoolBase<SharedPtrFactory, ResourceHealthCheck, invalidateResourceOnAccess>,
    /* Resource count keeper */
    public detail::ResourceCountKeeper<
        THIS_T,
        terminateOnResourceCountKeeperFailure,
        enableResourceCountKeeper
    >,
    /* Health care job */
    public detail::HealthCareJob<
    THIS_T,
        terminateOnHealthCareJobFailure,
        enableHealthCareJob
    >,
    /* Pool management */
    public PoolManagement<THIS_T>
#undef THIS_T
{
    static_assert(!(invalidateResourceOnAccess==true && enableHealthCareJob==false), "You cannot invalidate resource on access since there is no job to validate them again!");


    friend class detail::ResourceCountKeeper<
        SharedPtrPool,
        terminateOnResourceCountKeeperFailure,
        enableResourceCountKeeper
    >;

    friend class detail::HealthCareJob<
        SharedPtrPool,
        terminateOnHealthCareJobFailure,
        enableHealthCareJob
    >;

    using PoolManagement_t = PoolManagement<SharedPtrPool>;
    friend PoolManagement_t;


private:
    template<typename... FactoryArgs, typename... ResourceHealthCheckArgs, typename... PoolManagementArgs, typename LoggerSharedPtr, std::size_t... PI>
    SharedPtrPool(std::tuple<FactoryArgs...>&& factoryArgs,
                  std::tuple<ResourceHealthCheckArgs...>&& resourceHealthCheckArgs,
                  std::tuple<PoolManagementArgs...>&& poolManagementArgs,
                  std::index_sequence<PI...>,
                  LoggerSharedPtr&& loggerSharedPtr)
        : SharedPtrPoolBase<SharedPtrFactory, ResourceHealthCheck, invalidateResourceOnAccess>
          {
              std::move(factoryArgs),
              std::move(resourceHealthCheckArgs),
              std::forward<LoggerSharedPtr>(loggerSharedPtr)
          },
          PoolManagement_t
          {
              std::forward<PoolManagementArgs>(std::get<PI>(poolManagementArgs))...
          }
    {}


public:
    SharedPtrPool() = delete;
    SharedPtrPool(const SharedPtrPool&) = default;
    SharedPtrPool(SharedPtrPool&&) = default;
    SharedPtrPool& operator=(const SharedPtrPool&) = default;
    SharedPtrPool& operator=(SharedPtrPool&&) = default;
    ~SharedPtrPool() = default;

    template<typename... FactoryArgs, typename... ResourceHealthCheckArgs, typename... PoolManagementArgs,
             typename LoggerSharedPtr=decltype(DefaultLogger::getLoggerPtr())>
    SharedPtrPool(std::tuple<FactoryArgs...>&& factoryArgs,
                  std::tuple<ResourceHealthCheckArgs...>&& resourceHealthCheckArgs,
                  std::tuple<PoolManagementArgs...>&& poolManagementArgs,
                  LoggerSharedPtr&& loggerSharedPtr=DefaultLogger::getLoggerPtr())
        : SharedPtrPool
          {
              std::move(factoryArgs),
              std::move(resourceHealthCheckArgs),
              std::move(poolManagementArgs),
              std::make_index_sequence<sizeof...(PoolManagementArgs)>{},
              std::forward<LoggerSharedPtr>(loggerSharedPtr)
          }
    {}
};

} // end of namespace SuperiorMySqlpp
