/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <atomic>
#include <thread>
#include <chrono>
#include <future>
#include <memory>
#include <cassert>
#include <vector>
#include <mutex>
#include <algorithm>
#include <sstream>

#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/shared_ptr_pool/sleep_in_parts.hpp>



namespace SuperiorMySqlpp { namespace detail
{

template<typename Base, bool terminateOnFailure=false, bool /*enable*/=true>
class HealthCareJob
{
private:
    std::atomic<bool> enabled{false};
    std::chrono::milliseconds sleepTime{1000};
    unsigned int batchSize{5};

    std::thread jobThread{};

public:
    HealthCareJob() = default;

    HealthCareJob(const HealthCareJob&) = delete;

    HealthCareJob(HealthCareJob&& other)
        : enabled{other.enabled.load()},
          sleepTime{std::move(other).sleepTime}
    {
        other.stopHealthCareJob();

        if (enabled)
        {
            startHealthCareJob();
        }
    }

    HealthCareJob& operator=(const HealthCareJob&) = delete;
    HealthCareJob& operator=(HealthCareJob&&) = delete;

    ~HealthCareJob()
    {
        stopHealthCareJob();
    }


private:
    Base& getBase()
    {
        return *static_cast<Base*>(this);
    }

    const Base& getBase() const
    {
        return *static_cast<const Base*>(this);
    }

    /*
     * This function is run as parallel thread.
     */
    void job() const
    {
        using namespace std::string_literals;

        auto onError = [](){
            if (terminateOnFailure)
            {
                std::terminate();
            }
        };

        try
        {
            while (enabled)
            {
                getBase().getLogger()->logSharedPtrPoolHealthCareJobCycleStart(getBase().getId());

                auto&& pool = getBase().pool;
                auto&& poolMutex = getBase().poolMutex;

                // take pool's snapshot
                std::vector<typename Base::PoolItemWeak_t> poolSnapshot{};
                {
                    std::lock_guard<typename Base::PoolMutex_t> lock{poolMutex};
                    poolSnapshot.reserve(pool.size());

                    for (auto&& item: pool)
                    {
                        if (!Base::isUsed(item))
                        {
                            poolSnapshot.emplace_back(item.valid, std::weak_ptr<std::decay_t<decltype(*item.resource.get())>>{item.resource});
                        }
                    }
                }


                // go through snapshot and do health check
                std::vector<typename Base::PoolItemWeak_t> batchPool{};
                batchPool.reserve(poolSnapshot.size());
                typename Base::Pool_t batchPoolLocked{};
                batchPoolLocked.reserve(poolSnapshot.size());
                std::vector<std::tuple<std::future<bool>, typename Base::PoolItem_t>> batchHealthCheckFutures{};
                batchHealthCheckFutures.reserve(poolSnapshot.size());

                std::size_t i = 0;
                assert(batchSize>0);
                for (auto&& item: poolSnapshot)
                {
                    batchPool.emplace_back(item);
                    ++i;

                    if (i%batchSize==0 || i==poolSnapshot.size())
                    {
                        // try locking resources
                        for (auto&& weak: batchPool)
                        {
                            auto&& shared = weak.resource.lock();
                            // one reference is always owned by the main resource pool
                            if (shared && shared.use_count() == 2)
                            {
                                getBase().getLogger()->logSharedPtrPoolHealthCareJobLockedPtr(getBase().getId(), shared.get());
                                batchPoolLocked.emplace_back(weak.valid, std::move(shared));
                            }
                            else
                            {
                                getBase().getLogger()->logSharedPtrPoolHealthCareJobUnableToLockPtr(getBase().getId(), shared.get());
                            }
                        }
                        getBase().getLogger()->logSharedPtrPoolHealthCareJobLockedSize(getBase().getId(), batchPoolLocked.size());

                        // we are now the sole owners of these resources so we can call healthCheck
                        for (auto&& shared: batchPoolLocked)
                        {
                            getBase().getLogger()->logSharedPtrPoolHealthCareJobHealthCheckForPtr(getBase().getId(), shared.resource.get());

                            auto&& futureTuple = std::make_tuple(getBase().healthCheck(*shared.resource), shared);
                            batchHealthCheckFutures.emplace_back(std::move(futureTuple));
                        }

                        // we must pair healthCheck results and remove those, who failed, from the main resource pool
                        for (auto&& result: batchHealthCheckFutures)
                        {
                            try
                            {
                                auto&& isHealthy = std::get<0>(result).get();
                                auto&& checkedResource = std::get<1>(result);
                                if (!isHealthy)
                                // health check has failed
                                {
                                    getBase().getLogger()->logSharedPtrPoolHealthCareJobErasingPtr(getBase().getId(), checkedResource.resource.get());
                                    getBase().erase(std::move(checkedResource));
                                }
                                else
                                {
                                    if (Base::invalidateResourceOnAccess_)
                                    {
                                        if (!checkedResource.valid)
                                        {
                                            // Resource has not been valid before health-check => we shall update valid flag.
                                            using std::begin;
                                            using std::end;

                                            std::lock_guard<decltype(poolMutex)> lock{poolMutex};

                                            auto it = std::find_if(begin(pool), end(pool), [&checkedResource](auto&& item){
                                                return item.resource == checkedResource.resource;
                                            });

                                            if (it != end(pool))
                                            {
                                                it->valid = true;
                                            }
                                            else
                                            {
                                                // This is allowed to happen since user can clear pool while there are running health checks.
                                            }
                                        }
                                    }

                                    getBase().getLogger()->logSharedPtrPoolHealthCareJobLeavingHealthyResource(getBase().getId(), checkedResource.resource.get());
                                }
                            }
                            catch (const std::exception& e)
                            {
                                getBase().getLogger()->logSharedPtrPoolHealthCareJobHealthCheckError(getBase().getId(), e);
                            }
                            catch (...)
                            {
                                getBase().getLogger()->logSharedPtrPoolHealthCareJobHealthCheckError(getBase().getId());
                            }
                        }

                        getBase().getLogger()->logSharedPtrPoolHealthCareJobHealthCheckCompleted(getBase().getId(), batchHealthCheckFutures.size(), batchPoolLocked.size());

                        // erase pools
                        batchPool.clear();
                        batchPoolLocked.clear();
                        batchHealthCheckFutures.clear();
                    }
                }
                getBase().getLogger()->logSharedPtrPoolHealthCareJobCycleFinished(getBase().getId());

                sleepInParts(sleepTime, std::chrono::milliseconds{50}, [&](){ return enabled.load(); });
            }

            getBase().getLogger()->logSharedPtrPoolHealthCareJobStopped(getBase().getId());
        }
        catch (std::exception& e)
        {
            getBase().getLogger()->logSharedPtrPoolHealthCareJobError(getBase().getId(), e);
            onError();
        }
        catch (...)
        {
            getBase().getLogger()->logSharedPtrPoolHealthCareJobError(getBase().getId());
            onError();
        }
    }


public:
    void stopHealthCareJob()
    {
        enabled = false;
        if (jobThread.joinable())
        {
            jobThread.join();
        }
    }

    void startHealthCareJob()
    {
        /* We must stop possibly running thread otherwise ~thread/operator= shall call std::terminate! */
        stopHealthCareJob();
        enabled = true;
        jobThread = std::thread{&HealthCareJob::job, std::cref(*this)};
    }

    auto getHealthCareJobThreadId() const
    {
        return jobThread.get_id();
    }

    auto isHealthCareJobThreadRunning() const
    {
        return jobThread.joinable();
    }

    auto getHealthCareJobBatchSize() const
    {
        return batchSize;
    }

    void setHealthCareJobBatchSize(unsigned int size)
    {
        if (size <= 0)
        {
            throw OutOfRange{"BatchSize shall be > 0!!!"};
        }

        batchSize = size;
    }

    auto getHealthCareJobSleepTime() const
    {
        return sleepTime;
    }

    void setHealthCareJobSleepTime(decltype(sleepTime) value)
    {
        sleepTime = value;
    }
};

template<typename Base, bool terminateOnFailure>
class HealthCareJob<Base, terminateOnFailure, false>
{};


}}
