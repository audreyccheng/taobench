/*
 * Author: Tomas Nozicka
 */

#pragma once

/*
 * DO NOT include this header in superior_mysqpp.hpp or it will create dependency on boost.
 * This file also has separated automatic tests requiring Boost and elevated privileges.
 */


#include <chrono>


#include <superior_mysqlpp/connection_pool.hpp>
#include <superior_mysqlpp/shared_ptr_pool/sleep_in_parts.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/address.hpp>


namespace SuperiorMySqlpp
{
    namespace detail
    {
        class DnsResolver
        {
        private:

        public:
            DnsResolver() = default;
            DnsResolver(const DnsResolver&) = default;
            DnsResolver(DnsResolver&&) = default;
            ~DnsResolver() = default;

            DnsResolver& operator=(const DnsResolver&) = default;
            DnsResolver& operator=(DnsResolver&&) = default;

            std::vector<std::string> resolve(const std::string& hostname)
            {
                boost::asio::io_service ioService{};
                std::vector<std::string> addresses{};

                // setup the resolver query
                boost::asio::ip::tcp::resolver resolver{ioService};
                boost::asio::ip::tcp::resolver::query query{hostname, ""};

                // prepare response iterator
                boost::asio::ip::tcp::resolver::iterator destination{resolver.resolve(query)};
                boost::asio::ip::tcp::resolver::iterator end{};
                boost::asio::ip::tcp::endpoint endpoint{};

                while (destination != end)
                {
                    endpoint = *destination++;
                    addresses.emplace_back(endpoint.address().to_string());
                }

                return addresses;
            }
        };


        template<typename Base, bool terminateOnFailure>
        class DnsAwarePoolManagement
        {
        private:
            std::atomic<bool> enabled{false};
            std::chrono::milliseconds sleepTime{std::chrono::seconds{10}};
            std::thread jobThread{};

            std::string hostname;

        public:
            DnsAwarePoolManagement(std::string hostname)
                : hostname{std::move(hostname)}
            {}

            DnsAwarePoolManagement(const DnsAwarePoolManagement&) = delete;

            DnsAwarePoolManagement(DnsAwarePoolManagement&& other)
                : enabled{[&](){ other.stopDnsAwarePoolManagement(); return other.enabled.load(); }()},
                  sleepTime{std::move(other).sleepTime},
                  hostname{std::move(other).hostname}
            {
                if (enabled)
                {
                    startDnsAwarePoolManagement();
                }
            }

            ~DnsAwarePoolManagement()
            {
                stopDnsAwarePoolManagement();
            }

            DnsAwarePoolManagement& operator=(const DnsAwarePoolManagement&) = delete;
            DnsAwarePoolManagement& operator=(DnsAwarePoolManagement&&) = delete;

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
             * We cannot afford to let any exceptions to be propagated from logging functions
             * since it would result in call to std::terminate()!!! Hence all the try/catch blocks.
             */
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
                    std::vector<std::string> lastIpAddresses{};

                    while (enabled)
                    {
                        getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCycleStart(getBase().getId());
                        getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCycleStart(getBase().getId(), hostname);

                        try
                        {
                            /*
                             * We do intentionally make always new connection to DNS server.
                             */
                            DnsResolver resolver{};
                            auto ipAddresses = resolver.resolve(hostname);


                            // compare
                            if (!std::equal(lastIpAddresses.begin(), lastIpAddresses.end(), ipAddresses.begin(), ipAddresses.end()))
                            {
                                getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementChangeDetected(getBase().getId());
                                getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementChangeDetected(getBase().getId(), hostname);

                                getBase().clearPool();
                                lastIpAddresses = std::move(ipAddresses);
                            }
                        }
                        /*
                         * DNS is sometimes to fail...
                         */
                        catch (std::exception& e)
                        {
                            getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCheckError(getBase().getId(), e);
                            getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCheckError(getBase().getId(), e, hostname);
                        }
                        catch (...)
                        {
                            getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCheckError(getBase().getId());
                            getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCheckError(getBase().getId(), hostname);
                        }

                        getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCycleEnd(getBase().getId());
                        getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementCycleEnd(getBase().getId(), hostname);

                        sleepInParts(sleepTime, std::chrono::milliseconds{50}, [&](){ return enabled.load(); });
                    }

                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementStopped(getBase().getId());
                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementStopped(getBase().getId(), hostname);
                }
                catch (std::exception& e)
                {
                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementError(getBase().getId(), e);
                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementError(getBase().getId(), e, hostname);
                    onError();
                }
                catch (...)
                {
                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementError(getBase().getId());
                    getBase().getLogger()->logSharedPtrPoolDnsAwarePoolManagementError(getBase().getId(), hostname);
                    onError();
                }
            }

        public:
            void stopDnsAwarePoolManagement()
            {
                if (jobThread.joinable())
                {
                    enabled = false;
                    jobThread.join();
                }
                enabled = false;
            }

            void startDnsAwarePoolManagement()
            {
                /* We must stop possibly running thread otherwise ~thread/operator= shall call std::terminate! */
                stopDnsAwarePoolManagement();
                enabled = true;
                jobThread = std::thread{&DnsAwarePoolManagement::job, std::cref(*this)};
            }

            auto isDnsAwarePoolManagementThreadRunning() const
            {
                return jobThread.joinable();
            }

            auto getDnsAwarePoolManagementThreadId() const
            {
                return jobThread.get_id();
            }

            auto getDnsAwarePoolManagementSleepTime() const
            {
                return sleepTime;
            }

            void setDnsAwarePoolManagementSleepTime(decltype(sleepTime) value)
            {
                sleepTime = value;
            }
        };
    }


    template<typename Base>
    using DnsAwarePoolManagement = detail::DnsAwarePoolManagement<Base, true>;


    template<
        typename SharedPtrFactory,
        typename ResourceHealthCheck=void,
        bool invalidateResourceOnAccess=false,
        /* Resource count keeper */
        bool enableResourceCountKeeper=true,
        bool terminateOnResourceCountKeeperFailure=false,
        /* Health care job */
        bool enableHealthCareJob=true,
        bool terminateOnHealthCareJobFailure=false
    >
    using DnsaSharedPtrPool = SharedPtrPool<
            /* Basic pool */
            SharedPtrFactory,
            ResourceHealthCheck,
            invalidateResourceOnAccess,
            /* Resource count keeper */
            enableResourceCountKeeper,
            terminateOnResourceCountKeeperFailure,
            /* Health care job */
            enableHealthCareJob,
            terminateOnHealthCareJobFailure,
            /* Pool management */
            DnsAwarePoolManagement
    >;



    template<
        typename SharedPtrFactory,
        bool invalidateResourceOnAccess=false,
        /* Resource count keeper */
        bool enableResourceCountKeeper=true,
        bool terminateOnResourceCountKeeperFailure=false,
        /* Health care job */
        bool enableHealthCareJob=true,
        bool terminateOnHealthCareJobFailure=false
    >
    using DnsaConnectionPool = ConnectionPool<
            SharedPtrFactory,
            invalidateResourceOnAccess,
            /* Resource count keeper */
            enableResourceCountKeeper,
            terminateOnResourceCountKeeperFailure,
            /* Health care job */
            enableHealthCareJob,
            terminateOnHealthCareJobFailure,
            /* Pool management */
            DnsAwarePoolManagement
    >;


    template<
        bool invalidateResourceOnAccess=false,
        /* Resource count keeper */
        bool enableResourceCountKeeper=true,
        bool terminateOnResourceCountKeeperFailure=false,
        /* Health care job */
        bool enableHealthCareJob=true,
        bool terminateOnHealthCareJobFailure=false,
        typename SharedPtrFactory,
        typename LoggerPtr=decltype(DefaultLogger::getLoggerPtr())
    >
    auto makeDnsaConnectionPool(SharedPtrFactory&& factory, std::string hostname, LoggerPtr&& loggerPtr=DefaultLogger::getLoggerPtr())
    {
        return DnsaConnectionPool<
            std::decay_t<SharedPtrFactory>,
            invalidateResourceOnAccess,
            /* Resource count keeper */
            enableResourceCountKeeper,
            terminateOnResourceCountKeeperFailure,
            /* Health care job */
            enableHealthCareJob,
            terminateOnHealthCareJobFailure
        >
        {
            std::forward_as_tuple(std::forward<SharedPtrFactory>(factory)),
            std::forward_as_tuple(),
            std::forward_as_tuple(std::move(hostname)),
            std::forward<LoggerPtr>(loggerPtr)
        };
    }
}
