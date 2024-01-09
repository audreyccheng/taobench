/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <string>
#include <memory>
#include <utility>
#include <tuple>


#include <superior_mysqlpp/logging.hpp>
#include <superior_mysqlpp/prepared_statement_fwd.hpp>
#include <superior_mysqlpp/dynamic_prepared_statement_fwd.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/types/tags.hpp>
#include <superior_mysqlpp/types/optional.hpp>
#include <superior_mysqlpp/config.hpp>


namespace SuperiorMySqlpp
{
    class Query;

    using ClientFlags = LowLevel::DBDriver::ClientFlags;
    using ConnectionOptions = LowLevel::DBDriver::DriverOptions;


    class Connection
    {
    protected:
        LowLevel::DBDriver driver;

    protected:
        inline void setSslConfiguration(const SslConfiguration& sslConfig) noexcept
        {
            driver.setSsl(sslConfig.keyPath, sslConfig.certificatePath, sslConfig.certificationAuthorityPath,
                          sslConfig.trustedCertificateDirPath, sslConfig.allowableCiphers);
        }

        /**
         * Parses conection options.
         * The option tuples are not stored in the structure and are instead
         * directly passed into the callback
         */
        template <typename Callable>
        class OptionParser
        {
            Callable&& onOption;

            template <bool postConnect, typename TP1, typename TP2>
            void option(std::integral_constant<bool, postConnect>, const std::tuple<TP1, TP2>& input)
            {
                onOption(std::get<0>(input), std::get<1>(input));
            }

            template <bool postConnect, class TP1, class TP2>
            void option(std::integral_constant<bool, postConnect>, const std::pair<TP1, TP2>& input)
            {
                onOption(std::get<0>(input), std::get<1>(input));
            }

            template <bool postConnect>
            void option(std::integral_constant<bool, postConnect>, ClientFlags flag)
            {
                static_assert(!postConnect, "Client flags may only be set in Connection constructor.");
                clientFlags.flags |= flag.flags;
            }

            template<bool postConnect, typename... OptionTuples, std::size_t... I>
            void apply(std::integral_constant<bool, postConnect>, const std::tuple<OptionTuples...>& mainTuple, std::index_sequence<I...>)
            {
                // Prevent unused variable error if mainTuple is empty
                static_cast<void>(mainTuple);
                // Wrapped in initializer list to enforce ordering
                static_cast<void>(std::initializer_list<int>{(option(std::integral_constant<bool, postConnect>{}, std::get<I>(mainTuple)),0)...});
            }

        public:
            ClientFlags clientFlags{};

            template<bool postConnect, typename... OptionTuples>
            OptionParser(std::integral_constant<bool, postConnect>, const std::tuple<OptionTuples...>& optTuples, Callable&& onOption)
                : onOption{std::forward<Callable>(onOption)}
            {
                apply(std::integral_constant<bool, postConnect>{}, optTuples, std::index_sequence_for<OptionTuples...>{});
            }
        };

        /**
         * Helper because we can't autodeduce template params on OptionParser ctor.
         *
         * When postConnect flag is set to true, option setting happens after connection was opened
         * and settings that cannot be changed at that time aren't allowed.
         */
        template<bool postConnect = false, typename... OptionTuples, typename Callable>
        static OptionParser<Callable> makeOptionParser(const std::tuple<OptionTuples...>& optTuples, Callable&& onOption) {
            return {std::integral_constant<bool, postConnect>{}, optTuples, std::forward<Callable>(onOption)};
        }

    public:
        template<typename... OptionTuples>
        Connection(const std::string& database, const std::string& user, const std::string& password="",
                   const std::string& host="localhost", std::uint16_t port=3306,
                   std::tuple<OptionTuples...> optionTuples=std::make_tuple(),
                   Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : driver{std::move(loggerPtr)}
        {
            auto parser = makeOptionParser(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });
            driver.connect(host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, parser.clientFlags);
        }

        template<typename... OptionTuples>
        Connection(const SslConfiguration& sslConfig,
                   const std::string& database, const std::string& user, const std::string& password, const std::string& host, std::uint16_t port,
                   std::tuple<OptionTuples...> optionTuples=std::make_tuple(),
                   Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : driver{std::move(loggerPtr)}
        {
            auto parser = makeOptionParser(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });
            setSslConfiguration(sslConfig);
            driver.connect(host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, parser.clientFlags);
        }

        template<typename... OptionTuples>
        Connection(const std::string& database, const std::string& user, const std::string& password, const std::string& socketPath,
                   std::tuple<OptionTuples...> optionTuples=std::make_tuple(),
                   Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : driver{std::move(loggerPtr)}
        {
            auto parser = makeOptionParser(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });
            driver.connect(nullptr, user.c_str(), password.c_str(), database.c_str(), 0, socketPath.c_str(), parser.clientFlags);
        }

        template<typename... OptionTuples>
        Connection(const SslConfiguration& sslConfig,
                   const std::string& database, const std::string& user, const std::string& password, const std::string& socketPath,
                   std::tuple<OptionTuples...> optionTuples=std::make_tuple(),
                   Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : driver{std::move(loggerPtr)}
        {
            auto parser = makeOptionParser(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });
            setSslConfiguration(sslConfig);
            driver.connect(nullptr, user.c_str(), password.c_str(), database.c_str(), 0, socketPath.c_str(), parser.clientFlags);
        }

        template<typename... OptionTuples>
        Connection(ConnectionConfiguration config,
                   std::tuple<OptionTuples...> optionTuples=std::make_tuple(),
                   Loggers::SharedPointer_t loggerPtr=DefaultLogger::getLoggerPtr())
            : driver{std::move(loggerPtr)}
        {
            auto parser = makeOptionParser(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });

            if (config.sslConfig)
            {
                setSslConfiguration(config.sslConfig.value());
            }

            if (config.usingSocket)
            {
                driver.connect(nullptr, config.user.c_str(), config.password.c_str(), config.database.c_str(), 0, config.target.c_str(), parser.clientFlags);
            }
            else
            {
                driver.connect(config.target.c_str(), config.user.c_str(), config.password.c_str(), config.database.c_str(), config.port, nullptr, parser.clientFlags);
            }
        }


        ~Connection() = default;

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        Connection(Connection&&) = default;
        Connection& operator=(Connection&&) = default;


        auto& getLoggerPtr()
        {
            return driver.getLoggerPtr();
        }

        const auto& getLoggerPtr() const
        {
            return driver.getLoggerPtr();
        }

        template<typename T>
        void setLoggerPtr(T&& value)
        {
            driver.setLoggerPtr(std::forward<T>(value));
        }

        Loggers::ConstPointer_t getLogger() const
        {
            return driver.getLogger();
        }


        template<typename RBindings=ResultBindings<>,
                 bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
                 typename... Args>
        PreparedStatement<RBindings,
                          ParamBindings<std::decay_t<Args>...>,
                          storeResult,
                          validateMode,
                          warnMode,
                          ignoreNullable
                          > makePreparedStatement(const std::string&, Args&&...) && = delete;

        template<typename RBindings=ResultBindings<>,
                 bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
                 typename... Args>
        PreparedStatement<RBindings,
                          ParamBindings<std::decay_t<Args>...>,
                          storeResult,
                          validateMode,
                          warnMode,
                          ignoreNullable
                          > makePreparedStatement(const std::string&, Args&&...) &;



        template<typename RBindings=ResultBindings<>,
                 bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
                 template<typename...> class RArgsTuple, template<typename...> class PArgsTuple,
                 typename... RArgs, typename... PArgs
                 >
        PreparedStatement<RBindings,
                          ParamBindings<std::decay_t<PArgs>...>,
                          storeResult,
                          validateMode,
                          warnMode,
                          ignoreNullable
                          > makePreparedStatement(
                const std::string&, FullInitTag, RArgsTuple<RArgs...>&&, PArgsTuple<PArgs...>&&) && = delete;

        template<typename RBindings=ResultBindings<>,
                 bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable(),
                 template<typename...> class RArgsTuple, template<typename...> class PArgsTuple,
                 typename... RArgs, typename... PArgs
                 >
        PreparedStatement<RBindings,
                          ParamBindings<std::decay_t<PArgs>...>,
                          storeResult,
                          validateMode,
                          warnMode,
                          ignoreNullable
                          > makePreparedStatement(
                const std::string&, FullInitTag, RArgsTuple<RArgs...>&&, PArgsTuple<PArgs...>&&) &;




        template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable()>
        DynamicPreparedStatement<storeResult, validateMode, warnMode, ignoreNullable> makeDynamicPreparedStatement(const std::string& query) && = delete;

        template<bool storeResult=detail::PreparedStatementsDefault::getStoreResult(),
                 ValidateMetadataMode validateMode=detail::PreparedStatementsDefault::getValidateMode(),
                 ValidateMetadataMode warnMode=detail::PreparedStatementsDefault::getWarnMode(),
                 bool ignoreNullable=detail::PreparedStatementsDefault::getIgnoreNullable()>
        DynamicPreparedStatement<storeResult, validateMode, warnMode, ignoreNullable> makeDynamicPreparedStatement(const std::string& query) &;


        template<typename... Args>
        Query makeQuery(Args&&... args) && = delete;

        template<typename... Args>
        Query makeQuery(Args&&... args) &;


        std::string escapeString(const std::string& original)
        {
            return driver.escapeString(original);
        }

        static std::string escapeStringNoConnection(const std::string& original)
        {
            return LowLevel::DBDriver::escapeStringNoConnection(original);
        }

        void changeUser(const char* user, const char* password, const char* database)
        {
            driver.changeUser(user, password, database);
        }

        void ping()
        {
            driver.ping();
        }

        bool tryPing()
        {
            return driver.tryPing();
        }

        auto getId() const
        {
            return driver.getId();
        }

        using ServerOptions = LowLevel::DBDriver::ServerOptions;

        void setServerOption(ServerOptions option)
        {
            driver.setServerOption(option);
        }

        void setOption(ConnectionOptions option, const void* argumentPtr)
        {
            driver.setDriverOption(option, argumentPtr);
        }

        template<typename... OptionTuples>
        void setOptions(std::tuple<OptionTuples...> optionTuples)
        {
            auto parser = makeOptionParser<false>(std::move(optionTuples), [&](auto key, auto value) {
                this->setOption(key, value);
            });
            // Used for sideeffect
            (void)parser;
        }


        /*
         * DO NOT USE this function unless you want to work with wrapped C API directly!
         */
        LowLevel::DBDriver& detail_getDriver() && = delete;
        LowLevel::DBDriver& detail_getDriver() &
        {
            return driver;
        }
    };


    inline bool operator==(const Connection& lhs, const Connection& rhs)
    {
        return lhs.getId() == rhs.getId();
    }
}
