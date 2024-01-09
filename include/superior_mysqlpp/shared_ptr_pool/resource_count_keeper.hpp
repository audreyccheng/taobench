/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <atomic>
#include <thread>
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>

#include <superior_mysqlpp/shared_ptr_pool/sleep_in_parts.hpp>


namespace SuperiorMySqlpp { namespace detail
{


template<typename Base, bool terminateOnFailure=false, bool /*enable*/=true>
class ResourceCountKeeper
{
private:
    std::atomic<bool> enabled{false};
    std::size_t minSpare{10};
    std::size_t maxSpare{200};
    std::chrono::milliseconds sleepTime{100};

    std::thread jobThread{};


public:
    ResourceCountKeeper() = default;

    ResourceCountKeeper(const ResourceCountKeeper&) = delete;

    ResourceCountKeeper(ResourceCountKeeper&& other)
        : enabled{other.enabled.load()},
          minSpare{std::move(other).minSpare},
          maxSpare{std::move(other).maxSpare},
          sleepTime{std::move(other).sleepTime}
    {
        other.stopResourceCountKeeper();

        if (enabled)
        {
            startResourceCountKeeper();
        }
    }

    ResourceCountKeeper& operator=(const ResourceCountKeeper&) = delete;
    ResourceCountKeeper& operator=(ResourceCountKeeper&&) = delete;

    ~ResourceCountKeeper()
    {
        stopResourceCountKeeper();
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

    void job() const
    {
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
                getBase().getLogger()->logSharedPtrPoolResourceCountKeeperCycleStart(getBase().getId());

                std::unique_lock<decltype(getBase().poolMutex)> lock{getBase().poolMutex};
                auto poolState = getBase().poolStateUnsafe();
                auto availableCount = poolState.available;
                auto populationId = getBase().getPopulationId();
                lock.unlock();

                if (availableCount < minSpare)
                {
                    auto needed = minSpare - availableCount;

                    getBase().getLogger()->logSharedPtrPoolResourceCountKeeperTooLittleResources(getBase().getId(), availableCount, needed, poolState.used, poolState.size);

                    std::vector<std::future<typename Base::Resource_t>> futures{};
                    futures.reserve(needed);
                    for (std::size_t i=0; i<needed; ++i)
                    {
                        auto&& future = getBase().factory();
                        futures.emplace_back(std::move(future));
                    }

                    typename Base::Pool_t temporaryPool{};
                    temporaryPool.reserve(futures.size());
                    for (auto&& future: futures)
                    {
                        // C++ standard doesn't support has_exception like boost, so we must hack it a little
                        try
                        {
                            temporaryPool.emplace_back(true, std::move(future).get());
                        }
                        // ignore all errors
                        catch (std::exception& e)
                        {
                            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperAddingResourcesException(getBase().getId(), needed, e);
                        }
                        // ignore all errors
                        catch (...)
                        {
                            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperAddingResourcesException(getBase().getId(), needed);
                        }
                    }

                    {
                        std::unique_lock<decltype(getBase().poolMutex)> lock{getBase().poolMutex};
                        auto newPopulationId = getBase().getPopulationId();
                        if (populationId == newPopulationId)
                        {
                            std::move(temporaryPool.begin(), temporaryPool.end(), std::back_inserter(getBase().pool));
                            lock.unlock();
                            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperAddedResources(getBase().getId(), temporaryPool.size());
                        }
                        else
                        {
                            lock.unlock();
                            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperAdditionSkippedForNewPopulation(getBase().getId());
                        }
                    }
                }
                else if (availableCount > maxSpare)
                {
                    auto removeCount = availableCount - maxSpare;

                    getBase().getLogger()->logSharedPtrPoolResourceCountKeeperTooManyResources(getBase().getId(), availableCount, removeCount);

                    typename Base::Pool_t removePool{};
                    removePool.reserve(removeCount);

                    auto toRemove = removeCount;
                    {
                        std::lock_guard<decltype(getBase().poolMutex)> lock{getBase().poolMutex};

                        // first select non validated resources
                        auto it = std::remove_if(getBase().pool.begin(), getBase().pool.end(), [&](const auto& item){
                            if (toRemove > 0)
                            {
                                if (Base::isAvailable(item))
                                {
                                    --toRemove;
                                    return true;
                                }
                                else
                                {
                                    return false;
                                }
                            }
                            else
                            {
                                return false;
                            }
                        });

                        // now select any other
                        it = std::remove_if(getBase().pool.begin(), it, [&](const auto& item){
                            if (!Base::isUsed(item) && toRemove > 0)
                            {
                                --toRemove;
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                        });

                        std::move(it, getBase().pool.end(), std::back_inserter(removePool));

                        // since shared_ptr has been moved from they do not have associated memory
                        // and erasing them will not call resource destructors
                        getBase().pool.erase(it, getBase().pool.cend());
                        if (getBase().poolIndex >= getBase().pool.size())
                        {
                            getBase().poolIndex = 0;
                        }
                    }

                    getBase().getLogger()->logSharedPtrPoolResourceCountKeeperDisposingResources(getBase().getId(), removePool.size());

                    // destroy resources asynchronously
                    std::thread{[](typename Base::Pool_t&& removePool){
                        removePool.clear();
                    }, std::move(removePool)}.detach();
                }
                else
                {
                    getBase().getLogger()->logSharedPtrPoolResourceCountKeeperStateOK(getBase().getId(), availableCount, poolState.used, poolState.size);
                }

                sleepInParts(sleepTime, std::chrono::milliseconds{50}, [&](){ return enabled.load(); });
            }

            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperStoped(getBase().getId());
        }
        catch (std::exception& e)
        {
            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperError(getBase().getId(), e);
            onError();
        }
        catch (...)
        {
            getBase().getLogger()->logSharedPtrPoolResourceCountKeeperError(getBase().getId());
            onError();
        }
    }


public:
    void stopResourceCountKeeper()
    {
        if (jobThread.joinable())
        {
            enabled = false;
            jobThread.join();
        }
        else
        {
            enabled = false;
        }
    }

    void startResourceCountKeeper()
    {
        /* We must stop possibly running thread otherwise ~thread/operator= shall call std::terminate! */
        stopResourceCountKeeper();
        enabled = true;
        jobThread = std::thread{&ResourceCountKeeper::job, std::cref(*this)};
    }

    auto getMinSpare() const
    {
        return minSpare;
    }

    void setMinSpare(decltype(minSpare) value)
    {
        minSpare = value;
    }

    auto getMaxSpare() const
    {
        return maxSpare;
    }

    void setMaxSpare(decltype(maxSpare) value)
    {
        maxSpare = value;
    }

    auto isResourceCountKeeperThreadRunning() const
    {
        return jobThread.joinable();
    }

    auto getResourceCountKeeperThreadId() const
    {
        return jobThread.get_id();
    }

    auto getResourceCountKeeperSleepTime() const
    {
        return sleepTime;
    }

    void setResourceCountKeeperSleepTime(decltype(sleepTime) value)
    {
        sleepTime = value;
    }
};

template<typename Base, bool terminateOnFailure>
class ResourceCountKeeper<Base, terminateOnFailure, false>
{};


}}
