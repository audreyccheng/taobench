/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <vector>
#include <deque>
#include <queue>
#include <mutex>
#include <atomic>
#include <future>
#include <memory>
#include <algorithm>
#include <cassert>

#include <superior_mysqlpp/traits.hpp>
#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/types/tags.hpp>
#include <superior_mysqlpp/types/optional.hpp>



namespace SuperiorMySqlpp
{
namespace detail
{
    template<typename T>
    struct CheckedResouce
    {
        bool valid{true};
        T resource{};

        CheckedResouce() = default;
        CheckedResouce(const CheckedResouce&) = default;
        CheckedResouce(CheckedResouce&&) = default;
        CheckedResouce& operator=(const CheckedResouce&) = default;
        CheckedResouce& operator=(CheckedResouce&&) = default;
        ~CheckedResouce() = default;

        template<typename U>
        CheckedResouce(bool valid, U&& resource)
            : valid{valid}, resource{std::forward<U>(resource)}
        {}
    };


    template<typename T>
    bool operator==(const CheckedResouce<T>& lhs, const CheckedResouce<T>& rhs)
    {
        return lhs.valid == rhs.valid && lhs.resource == rhs.resource;
    }


    template<typename Callable>
    class HealthCheck
    {
    private:
        Callable healthCheckCallable;

    public:
        HealthCheck() = default;
        HealthCheck(const HealthCheck&) = default;
        HealthCheck(HealthCheck&&) = default;
        HealthCheck& operator=(const HealthCheck&) = default;
        HealthCheck& operator=(HealthCheck&&) = default;

        template<typename... Args>
        explicit HealthCheck(Args&&... args)
        : healthCheckCallable{std::forward<Args>(args)...}
        {}

        template<typename T>
        std::future<bool> healthCheck(T&& resource) const
        {
            return healthCheckCallable(std::forward<T>(resource));
        }
    };

    template<>
    class HealthCheck<void>
    {
    public:
        HealthCheck() = default;
        HealthCheck(const HealthCheck&) = default;
        HealthCheck(HealthCheck&&) = default;
        HealthCheck& operator=(const HealthCheck&) = default;
        HealthCheck& operator=(HealthCheck&&) = default;

        template<typename T>
        std::future<bool> healthCheck(T&&) const
        {
            std::promise<bool> promise{};
            promise.set_value(true);
            return promise.get_future();
        }
    };
}


struct SharedPtrPoolState
{
    std::size_t size{0};
    std::size_t available{0};  // does not include returned and not validated
    std::size_t used{0};
};

struct SharedPtrPoolFullState : SharedPtrPoolState
{
    std::size_t unhealthy;
};


template<typename T, typename=void>
struct IsFutureConcept : std::false_type
{};

template<typename T>
struct IsFutureConcept<T, Traits::Void_t<
    decltype(std::declval<T>().share()),
    decltype(std::declval<T>().get()),
    decltype(std::declval<T>().valid()),
    decltype(std::declval<T>().wait()),
    decltype(std::declval<T>().wait_for(std::declval<std::chrono::seconds>())),
    decltype(std::declval<T>().wait_until(std::declval<std::chrono::system_clock::time_point>()))
>> : std::true_type
{};


namespace detail
{
    inline auto& getSharedPtrPoolGlobalIdRef()
    {
        static std::atomic<std::uint_fast64_t> globalId{1};
        return globalId;
    }

    inline auto getSharedPtrPoolNewGlobalId()
    {
        return getSharedPtrPoolGlobalIdRef().fetch_add(1);
    }
}

template<
    typename SharedPtrFactory,
    typename ResourceHealthCheck,
    bool invalidateResourceOnAccess,
    // concepts
    typename std::enable_if<IsFutureConcept<std::result_of_t<SharedPtrFactory()>>::value, int>::type=0
    >
class SharedPtrPoolBase : public detail::HealthCheck<ResourceHealthCheck>
{
public:
    using Resource_t = std::decay_t<decltype(std::declval<SharedPtrFactory>()().get())>;
    using PoolItem_t = detail::CheckedResouce<Resource_t>;
    using PoolItemWeak_t = detail::CheckedResouce<std::weak_ptr<std::decay_t<decltype(*std::declval<Resource_t>().get())>>>;
    using Pool_t = std::vector<PoolItem_t>;
    using PoolMutex_t = std::mutex;

    static constexpr bool invalidateResourceOnAccess_ = invalidateResourceOnAccess;

private:
    const std::uint_fast64_t id;

protected:
    mutable std::size_t poolIndex;
    mutable Pool_t pool;
    mutable PoolMutex_t poolMutex;
    mutable std::atomic<unsigned int> populationId{0};

protected:
    SharedPtrFactory factory;

protected:
    Loggers::SharedPointer_t loggerSharedPtr;

private:
    template<typename... FactoryArgs, typename... HealthCheckArgs, typename LoggerSharedPtr, std::size_t... FI, std::size_t... HI>
    explicit SharedPtrPoolBase(std::tuple<FactoryArgs...>& factoryArgs,
                               std::index_sequence<FI...>,
                               std::tuple<HealthCheckArgs...>& healthCheckArgs,
                               std::index_sequence<HI...>,
                               LoggerSharedPtr&& loggerSharedPtr)
        : detail::HealthCheck<ResourceHealthCheck>{std::forward<HealthCheckArgs>(std::get<HI>(healthCheckArgs))...},
          id{detail::getSharedPtrPoolNewGlobalId()},
          poolIndex{0},
          pool{},
          poolMutex{},
          factory{std::forward<FactoryArgs>(std::get<FI>(factoryArgs))...},
          loggerSharedPtr{std::forward<LoggerSharedPtr>(loggerSharedPtr)}
    {}

public:
    template<typename... FactoryArgs, typename... HealthCheckArgs, typename LoggerSharedPtr>
    SharedPtrPoolBase(std::tuple<FactoryArgs...> factoryArgs, std::tuple<HealthCheckArgs...> healthCheckArgs, LoggerSharedPtr&& loggerSharedPtr)
        : SharedPtrPoolBase
          {
              factoryArgs,
              std::make_index_sequence<sizeof...(FactoryArgs)>{},
              healthCheckArgs,
              std::make_index_sequence<sizeof...(HealthCheckArgs)>{},
              std::forward<LoggerSharedPtr>(loggerSharedPtr)
          }
    {}


    SharedPtrPoolBase(SharedPtrPoolBase&& other)
        : detail::HealthCheck<ResourceHealthCheck>{static_cast<detail::HealthCheck<ResourceHealthCheck>&&>(other)},
          id{std::move(other).id},
          poolIndex{std::move(other).poolIndex},
          pool{std::move(other).pool},
          poolMutex{},
          factory{std::move(other).factory},
          loggerSharedPtr{std::move(other).loggerSharedPtr}
    {}

    SharedPtrPoolBase(const SharedPtrPoolBase&) = delete;
    SharedPtrPoolBase& operator=(const SharedPtrPoolBase&) = delete;
    SharedPtrPoolBase& operator=(SharedPtrPoolBase&& other) = delete;
    ~SharedPtrPoolBase() = default;


private:
    void eraseUnsafe(const PoolItem_t& item) const
    {
        auto it = std::remove_if(pool.begin(), pool.end(), [&](const PoolItem_t& current){ return current.resource == item.resource; });
        getLogger()->logSharedPtrPoolErasingResource(id, it->resource.get());
        pool.erase(it);
        if (poolIndex >= pool.size())
        {
            poolIndex = 0;
        }
    }

protected:
    Loggers::ConstPointer_t getLogger() const
    {
        return loggerSharedPtr.get();
    }


public:
    auto getId() const
    {
        return id;
    }

    unsigned int getPopulationId() const
    {
        return populationId;
    }

    void setPopulationId(unsigned int id) const
    {
        populationId = id;
    }


    static bool isUsed(const PoolItem_t& item)
    {
        return item.resource.use_count() > 1;
    };

    static bool isAvailable(const PoolItem_t& item)
    {
        return item.valid && !isUsed(item);
    };


    auto poolStateUnsafe() const
    {
        SharedPtrPoolState state{};

        state.size = pool.size();
        state.available = std::count_if(pool.cbegin(), pool.cend(), [&](const auto& item){ return this->isAvailable(item); });
        state.used = std::count_if(pool.cbegin(), pool.cend(), [&](const auto& item){ return this->isUsed(item); });

        return state;
    }

    auto getPoolSnapshotUnsafe() const
    {
        std::vector<PoolItemWeak_t> result{};
        result.reserve(pool.size());

        std::copy(pool.cbegin(), pool.cend(), result.begin());

        return result;
    }


public:
    auto& getLoggerSharedPtr()
    {
        return loggerSharedPtr;
    }

    const auto& getLoggerSharedPtr() const
    {
        return loggerSharedPtr;
    }

    template<typename T>
    void setLoggerSharedPtr(T&& value)
    {
        loggerSharedPtr = std::forward<T>(value);
    }


    Resource_t get() const
    {
        std::unique_lock<PoolMutex_t> lock{poolMutex};
        auto populationId = getPopulationId();

        assert((pool.size()==0 && poolIndex==0) || (pool.size()>0 && poolIndex<pool.size()));

        auto poolIndexIt = pool.begin() + poolIndex;

        auto it = std::find_if(poolIndexIt, pool.end(), [&](const auto& item){ return this->isAvailable(item); });
        bool found = (it != pool.end());

        if (!found && poolIndex > 0)
        {
            it = std::find_if(pool.begin(), poolIndexIt, [&](const auto& item){ return this->isAvailable(item); });
            found = (poolIndexIt != poolIndexIt);
        }

        if (found)
        {
            poolIndex = it - pool.begin();
            if (invalidateResourceOnAccess)
            {
                it->valid = false;
            }
            return it->resource;
        }
        else
        {
            lock.unlock();

            getLogger()->logSharedPtrPoolEmergencyResourceCreation(id);
            auto futureResource = factory();
            auto newResource = std::move(futureResource).get();

            lock.lock();

            auto newPopulationId = getPopulationId();

            if (populationId == newPopulationId)
            {
                pool.emplace_back(!invalidateResourceOnAccess, newResource);

                assert(pool.size() > 0);
                poolIndex = pool.size() - 1;

                getLogger()->logSharedPtrPoolEmergencyResourceAdded(id);
            }
            else
            {
                getLogger()->logSharedPtrPoolEmergencyResourceAdditionSkippedForNewPopulation(id);
            }

            return newResource;
        }
    }

    std::future<Resource_t> getUnpooledFuture() const
    {
        return factory();
    }

    Resource_t getUnpooled() const
    {
        return getUnpooledFuture().get();
    }


    void erase(const PoolItem_t& item) const
    {
        std::lock_guard<PoolMutex_t> lock{poolMutex};
        eraseUnsafe(item);
    }


    auto poolState() const
    {
        std::lock_guard<PoolMutex_t> lock{poolMutex};
        return poolStateUnsafe();
    }


    /*
     * Use this method only for debugging purposes since it will lock the pool for as long as all health checks take place!!!
     */
    auto poolFullState() const
    {
        std::lock_guard<PoolMutex_t> lock{poolMutex};
        SharedPtrPoolFullState state{};

        static_cast<SharedPtrPoolState&>(state) = poolStateUnsafe();

        std::vector<std::future<bool>> futures{};
        futures.reserve(pool.size());

        for (const auto& item: pool)
        {
            if (!isUsed(item))
            {
                assert(item.resource.use_count()==1);
                futures.emplace_back(this->healthCheck(*item.resource));
            }
        }

        state.unhealthy = 0;
        for (auto&& future: futures)
        {
            if (!std::move(future).get())
            {
                ++state.unhealthy;
            }
        }

        return state;
    }


    auto countUnhealthyResources() const
    {
        SharedPtrPoolState state{};
        std::lock_guard<PoolMutex_t> lock{poolMutex};

        state.size = pool.size();
        state.available = std::count_if(pool.cbegin(), pool.cend(), isAvailable);
        state.used = std::count_if(pool.cbegin(), pool.cend(), isUsed);

        return state;
    }


    void clearPool() const
    {
        std::unique_lock<PoolMutex_t> lock{poolMutex};
        getLogger()->logSharedPtrPoolClearPool(id);
        Pool_t tmpPool{std::move(pool)};
        pool.clear();
        poolIndex = 0;
        lock.unlock();  // unlock is essential in this place since dtor of tmpPool may take time
    }


    /*
     * This is intended only for managements jobs.
     * Do not use this otherwise!!!
     */
    auto getPoolSnapshot() const
    {
        std::lock_guard<PoolMutex_t> lock{poolMutex};
        return getPoolSnapshotUnsafe();
    }


    /*
     * Do not use this if you can!
     */
    auto& detail_getPool()
    {
        return pool;
    }

    /*
     * Do not use this if you can!
     */
    auto& detail_getPoolMutex()
    {
        return poolMutex;
    }

    /*
     * Do not use this if you can!
     */
    auto& detail_getPoolIndex()
    {
        return poolIndex;
    }
};


} // end of namespace SuperiorMySqlpp
