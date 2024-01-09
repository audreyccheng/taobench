/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <utility>
#include <vector>
#include <map>

#include <superior_mysqlpp/exceptions.hpp>
#include <superior_mysqlpp/connection_pool.hpp>
#include <superior_mysqlpp/types/special_iterator.hpp>
#include <superior_mysqlpp/types/concat_iterator.hpp>


namespace SuperiorMySqlpp
{
    const auto mapSecondGetter = [](auto&& item) -> auto& { return std::forward<decltype(item)>(item).second; };

    template<
        typename SharedPtrPoolType,
        typename SlaveId=unsigned int
    >
    class MasterSlaveSharedPtrPools
    {
    public:
        using Pool_t = SharedPtrPoolType;

    private:
        Pool_t master;
        std::map<SlaveId, Pool_t> slaves;

    public:
        template<typename... MasterArgs>
        explicit MasterSlaveSharedPtrPools(MasterArgs&&... masterArgs)
            : master{std::forward<MasterArgs>(masterArgs)...}
        {
        }

        MasterSlaveSharedPtrPools(const MasterSlaveSharedPtrPools&) = delete;
        MasterSlaveSharedPtrPools& operator=(const MasterSlaveSharedPtrPools&) = delete;

        MasterSlaveSharedPtrPools(MasterSlaveSharedPtrPools&&) = default;
        MasterSlaveSharedPtrPools& operator=(MasterSlaveSharedPtrPools&&) = default;


    public:
        /*
         * Operations on master pool
         */
        template<typename Pool>
        void setMasterPool(Pool&& masterPool)
        {
            master = std::forward<Pool>(masterPool);
        }

        Pool_t& getMasterPoolRef()
        {
            return master;
        }

        const Pool_t& getMasterPoolRef() const
        {
            return master;
        }

        auto getMasterConnection() const
        {
            return getMasterPoolRef().get();
        }

        auto getMasterUnpooledConnectionFuture() const
        {
            return getMasterPoolRef().getUnpooledFuture();
        }

        auto getMasterUnpooledConnection() const
        {
            return getMasterPoolRef().getUnpooled();
        }


        /*
         * Operations on slave pools
         */
        template<typename Id, typename SlavePool>
        auto& addSlavePool(Id&& slaveId, SlavePool&& slavePool)
        {
            auto result = slaves.insert(std::make_pair(std::forward<Id>(slaveId), std::forward<SlavePool>(slavePool)));
            if (!result.second)
            {
                throw LogicError{"Slave pool with this id already exists!"};
            }

            return result.first->second;
        }

        template<typename Id, typename... Args>
        auto& emplaceSlavePool(Id&& slaveId, Args&&... args)
        {
            auto result = slaves.emplace(std::forward<Id>(slaveId), std::forward<Args>(args)...);
            if (!result.second)
            {
                throw LogicError{"Slave pool with this id already exists!"};
            }

            return result.first->second;
        }

        template<typename Id>
        void eraseSlavePool(const Id& slaveId)
        {
            auto&& it = slaves.find(slaveId);
            if (it == slaves.end())
            {
                throw LogicError{"No slave pool with this id has been found!"};
            }
            else
            {
                slaves.erase(it);
            }
        }

        const Pool_t& getSlavePoolRef() const
        {
            if (slaves.size() == 0)
            {
                return master;
            }
            else
            {
                return std::get<1>(*slaves.begin());
            }
        }

        Pool_t& getSlavePoolRef()
        {
            return const_cast<Pool_t&>(const_cast<const MasterSlaveSharedPtrPools*>(this)->getSlavePoolRef());
        }

        template<typename Id>
        const Pool_t& getSlavePoolRef(const Id& slaveId) const
        {
            auto&& it = slaves.find(slaveId);
            if (it == std::end(slaves))
            {
                throw LogicError{"No slave pool with this id has been found!"};
            }
            else
            {
                return it->second;
            }
        }

        template<typename Id>
        Pool_t& getSlavePoolRef(const Id& slaveId)
        {
            return const_cast<Pool_t&>(const_cast<const MasterSlaveSharedPtrPools*>(this)->getSlavePoolRef(slaveId));
        }

        template<typename Id>
        const Pool_t& getSlaveOrMasterPoolRef(const Id& slaveId) const
        {
            auto&& it = slaves.find(slaveId);
            if (it == std::end(slaves))
            {
                return master;
            }
            else
            {
                return it->second;
            }
        }

        template<typename Id>
        Pool_t& getSlaveOrMasterPoolRef(const Id& slaveId)
        {
            return const_cast<Pool_t&>(const_cast<const MasterSlaveSharedPtrPools*>(this)->getSlaveOrMasterPoolRef(slaveId));
        }


        auto getSlaveConnection() const
        {
            return getSlavePoolRef().get();
        }

        template<typename Id>
        auto getSlaveConnection(const Id& slaveId) const
        {
            return getSlavePoolRef(slaveId).get();
        }

        template<typename Id>
        auto getSlaveOrMasterConnection(const Id& slaveId) const
        {
            return getSlaveOrMasterPoolRef(slaveId).get();
        }

        auto getSlaveUnpooledConnection() const
        {
            return getSlavePoolRef().getUnpooled();
        }

        template<typename Id>
        auto getSlaveUnpooledConnection(const Id& slaveId) const
        {
            return getSlavePoolRef(slaveId).getUnpooled();
        }

        template<typename Id>
        auto getSlaveOrMasterUnpooledConnection(const Id& slaveId) const
        {
            return getSlaveOrMasterPoolRef(slaveId).getUnpooled();
        }

        auto getSlaveUnpooledConnectionFuture() const
        {
            return getSlavePoolRef().getUnpooledFuture();
        }

        template<typename Id>
        auto getSlaveUnpooledConnectionFuture(const Id& slaveId) const
        {
            return getSlavePoolRef(slaveId).getUnpooledFuture();
        }

        template<typename Id>
        auto getSlaveOrMasterUnpooledConnectionFuture(const Id& slaveId) const
        {
            return getSlaveOrMasterPoolRef(slaveId).getUnpooledFuture();
        }


    public:
        auto begin()
        {
            return makeConcatIterator(
                    &master, &master+1,
                    makeSpecialIterator(slaves.begin(), mapSecondGetter), makeSpecialIterator(slaves.end(), mapSecondGetter),
                    firstTag, &master
            );
        }

        auto end()
        {
            return makeConcatIterator(
                    &master, &master+1,
                    makeSpecialIterator(slaves.begin(), mapSecondGetter), makeSpecialIterator(slaves.end(), mapSecondGetter),
                    secondTag, makeSpecialIterator(slaves.end(), mapSecondGetter)
            );
        }

        auto cbegin() const
        {
            return makeConcatIterator(
                    &master, &master+1,
                    makeSpecialIterator(slaves.cbegin(), mapSecondGetter), makeSpecialIterator(slaves.cend(), mapSecondGetter),
                    firstTag, &master
            );
        }

        auto cend() const
        {
            return makeConcatIterator(
                    &master, &master+1,
                    makeSpecialIterator(slaves.cbegin(), mapSecondGetter), makeSpecialIterator(slaves.cend(), mapSecondGetter),
                    secondTag, makeSpecialIterator(slaves.cend(), mapSecondGetter)
            );
        }

        auto begin() const
        {
            return cbegin();
        }

        auto end() const
        {
            return cend();
        }

        auto size() const
        {
            return slaves.size() + 1;
        }


    private:
        class SlavesCollection
        {
        private:
            MasterSlaveSharedPtrPools& parent;

        public:
            SlavesCollection(MasterSlaveSharedPtrPools& parent)
                : parent{parent}
            {
            }

        public:
            auto begin()
            {
                return makeSpecialIterator(parent.slaves.begin(), mapSecondGetter);
            }

            auto end()
            {
                return makeSpecialIterator(parent.slaves.end(), mapSecondGetter);
            }

            auto cbegin() const
            {
                return makeSpecialIterator(parent.slaves.cbegin(), mapSecondGetter);
            }

            auto cend() const
            {
                return makeSpecialIterator(parent.slaves.cend(), mapSecondGetter);
            }

            auto begin() const
            {
                return cbegin();
            }

            auto end() const
            {
                return cend();
            }

            auto size() const
            {
                return parent.slaves.size();
            }
        };

    public:
        SlavesCollection slavesCollection{*this};
    };


    template<typename SlaveId=int, typename SharedPtrPoolType, typename LoggerPtr=decltype(DefaultLogger::getLoggerPtr())>
    auto makeMasterSlaveSharedPtrPools(SharedPtrPoolType&& factory, LoggerPtr&& loggerPtr=DefaultLogger::getLoggerPtr())
    {
        return MasterSlaveSharedPtrPools<std::decay_t<SharedPtrPoolType>, SlaveId>(
            std::forward_as_tuple(std::forward<SharedPtrPoolType>(factory)),
            std::forward_as_tuple(),
            std::forward_as_tuple(),
            std::forward<LoggerPtr>(loggerPtr)
        );
    }


    template<typename SharedPtrFactory, typename SlaveId=int>
    using MasterSlaveConnectionPools = MasterSlaveSharedPtrPools<ConnectionPool<SharedPtrFactory>, SlaveId>;

    template<typename SlaveId=int, typename SharedPtrFactory, typename LoggerPtr=decltype(DefaultLogger::getLoggerPtr())>
    auto makeMasterSlaveConnectionPools(SharedPtrFactory&& factory, LoggerPtr&& loggerPtr=DefaultLogger::getLoggerPtr())
    {
        return MasterSlaveConnectionPools<std::decay_t<SharedPtrFactory>, SlaveId>(
            std::forward_as_tuple(std::forward<SharedPtrFactory>(factory)),
            std::forward_as_tuple(),
            std::forward_as_tuple(),
            std::forward<LoggerPtr>(loggerPtr)
        );
    }
}
