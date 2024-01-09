/*
 * Author: Jiří Kozlovský
 */

#pragma once

#include <string>

#include <superior_mysqlpp/types/optional.hpp>

namespace SuperiorMySqlpp
{
    struct SslConfiguration
    {
        const char* keyPath = nullptr;
        const char* certificatePath = nullptr;
        const char* certificationAuthorityPath = nullptr;
        const char* trustedCertificateDirPath = nullptr;
        const char* allowableCiphers = nullptr;
    };

    class ConnectionConfiguration {
        private:
            ConnectionConfiguration(const Optional<SslConfiguration>& _sslConfig, const std::string& _database, const std::string& _user, const std::string& _password,
                    const std::string& _target, const std::uint16_t _port=0) :
                sslConfig{_sslConfig},
                usingSocket{_port == 0},
                database{_database},
                user{_user},
                password{_password},
                target{_target},
                port{_port}
            { }

        public:

            const Optional<SslConfiguration> sslConfig;

            const bool usingSocket;

            const std::string database;
            const std::string user;
            const std::string password;
            const std::string target;
            const std::uint16_t port;


            static ConnectionConfiguration getSslTcpConnectionConfiguration(const SslConfiguration& sslConfig, const std::string& database,
                    const std::string& user, const std::string& password, const std::string& host="localhost", std::uint16_t port=3306)
            {
                    return ConnectionConfiguration{sslConfig, database, user, password, host, port};
            }

            static ConnectionConfiguration getTcpConnectionConfiguration(const std::string& database, const std::string& user, const std::string& password,
                    const std::string& host="localhost", std::uint16_t port=3306)
            {
                    return ConnectionConfiguration{{}, database, user, password, host, port};
            }

            static ConnectionConfiguration getSslSocketConnectionConfiguration(const SslConfiguration& sslConfig, const std::string& database,
                    const std::string& user, const std::string& password, const std::string& socket="/var/run/mysqld/mysqld.sock")
            {
                    return ConnectionConfiguration{sslConfig, database, user, password, socket};
            }

            static ConnectionConfiguration getSocketConnectionConfiguration(const std::string& database, const std::string& user, const std::string& password,
                    const std::string& socket="/var/run/mysqld/mysqld.sock")
            {
                    return ConnectionConfiguration{{}, database, user, password, socket};
            }
    };
}
