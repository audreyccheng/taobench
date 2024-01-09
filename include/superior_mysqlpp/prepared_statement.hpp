/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <superior_mysqlpp/prepared_statement_fwd.hpp>

#include <array>
#include <tuple>
#include <type_traits>
#include <utility>
#include <cstring>
#include <cinttypes>
#include <memory>

#include <superior_mysqlpp/prepared_statements/initialize_bindings.hpp>
#include <superior_mysqlpp/prepared_statements/prepared_statement_base.hpp>
#include <superior_mysqlpp/prepared_statements/default_initialize_result.hpp>
#include <superior_mysqlpp/low_level/dbdriver.hpp>
#include <superior_mysqlpp/connection_def.hpp>
#include <superior_mysqlpp/metadata.hpp>
#include <superior_mysqlpp/traits.hpp>
#include <superior_mysqlpp/types/tags.hpp>

/************************************
 * Common usages of PreparedStatement
 ************************************
 *
 *
 *  Notes - We prefer 'Sql::Int' over 'int' for portability.
 *          Empty ResultBindings can be ommited entirely.
 *          Storage of parameter values is always inside prepared statement, even if lvalues are passed for initialization.
 *          Parameter values are passed in varargs fashion and type of ParamBindings is inferred from that.
 *            So, you are generally expected to provide suitable values of parameters at a time of construction.
 *          Additional template parameters of makePreparedStatement allow further customization.
 *
 *
 *  Step - creating prepared statement
 *
 * ``` c++
 * SuperiorMySqlpp::Connection connection;
 * ...
 *
 * // case a) No parameters
 * auto preparedStatement = connection.makePreparedStatement<ResultBindings<Sql::Int, Sql::Int>>(
 *     "SELECT `id`, `money` FROM `transactions`"
 * );
 * // case b) No result, 4 parameters
 * auto preparedStatement = connection.makePreparedStatement(
 *     "INSERT INTO `tablename` VALUES (?, ?), (?, ?)",
 * 	42, Nullable<StringDataBase<12>>{"Hello"},
 * 	43, Nullable<StringDataBase<12>>{"world"}
 * );
 * // case c) Deferred parameters - using PreparedStatement ctor directly
 * auto preparedStatement = PreparedStatement<ResultBindings<Sql::TinyInt>,ParamBindings<Sql::Int, Sql::Int>>(
 * 	connection,
 *     "INSERT INTO `tablename` VALUES (?, ?)"
 * );
 * ```
 *
 *  Step - execution
 * ``` c++
 * // This actually performs the query on the server.
 * // May be called multiple times
 *
 * preparedStatement.execute();
 * ```
 *
 *  Step - data retrieval
 *    - needed only for SELECT(-like) statements
 *
 * ``` c++
 * // Fetches single row into prepared statement's storage
 * while (preparedStatement.fetch()) // This is true as long as data are available (throws on errors like truncation)
 * {
 * 	Sql::Int id, money;
 *
 * 	// Now, one can read std::tuple within prepared statement directly
 * 	std::tie(id, money) = preparedStatement.getResult();
 * }
 * ```
 *
 * // Optional step - if we decide to change parameters (shall happen before execution)
 * ``` c++
 * preparedStatement.setParams(123,StringData{"Xyzzy"});
 * preparedStatement.updateParamsBindings(); // Updates binding information, required step for most nontrivial parameter types
 * preparedStatement.execute(); // Statement is performed now
 * preparedStatement.setParams(666,StringData{"Elbereth"});
 * preparedStatement.updateParamsBindings();
 * preparedStatement.execute(); // Statement is performed again, now with different parameters
 * ```
 **/


namespace SuperiorMySqlpp
{
    /**
     * @brief Bindings of storage for either parameters (inputs) or results (outputs) of SQL queries.
     * Bindings class encapsulates the storage of heterogenous data needed for SQL parameters and results.
     * The underlying storage itself is in std::tuple member data, auxiliary binding information is stored
     * in array of MYSQL_BIND structures used from underlying C mysql client -- member bindings.
     * @tparam IsParamBinding Special flag for template specialization.
     *                        If Bindings is used for query parameter, it is true.
     *                        If Bindings is used for query result, it is false.
     * @tparam Types Parameter pack of types forming storage of individual fields.
     * @remark In user code, prefer use convenience aliases ParamBindings and ResultBindings.
     */
    template<bool IsParamBinding, typename... Types>
    class Bindings
    {
    public:
        static constexpr auto kArgumentsCount = sizeof...(Types);
        static constexpr bool isParamBinding = IsParamBinding;

        /** Underlying storage for binding's data. */
        std::tuple<Types...> data;
        /** Array of binding structures pointing to storage in member #data. */
        std::array<MYSQL_BIND, sizeof...(Types)> bindings;


    public:
        /**
         * Bindings constructor.
         * @params args Args is a pack of values used for initialization of binding's storage.
         * @remark using just 'bindings{}' triggers (premature) warning in GCC 4.9.2,
         *         see discussion at https://stackoverflow.com/questions/31271975/why-can-i-initialize-a-regular-array-from-but-not-a-stdarray
         */
        template<typename... Args>
        Bindings(Args&&... args)
            : data{std::forward<Args>(args)...}, bindings{{}}
        {
            static_assert(sizeof...(Args) == 0 || sizeof...(Args) == sizeof...(Types), "You shall initialize all bindings data or none of them!");
            update();
        }

        Bindings(const Bindings&) = default;
        Bindings(Bindings&&) = default;
        Bindings& operator=(const Bindings&) = default;
        Bindings& operator=(Bindings&&) = default;
        ~Bindings() = default;

        /**
         * Updates bindings to properly reflect on data.
         * Is fundamental for useability of Bindings and is thus called during initialization.
         */
        void update()
        {
            detail::initializeBindings<IsParamBinding, Types...>(bindings, data);
        }
    };

    /**
     * ParamBindings specifies bindings that shall be used for query parameters.
     * @tparam Types List of types matching query params.
     */
    template<typename... Types>
    using ParamBindings = Bindings<true, Types...>;

    /**
     * ResultBindings specifies bindings that shall be used for query results.
     * @tparam Types List of types matching query results.
     */
    template<typename... Types>
    using ResultBindings = Bindings<false, Types...>;

    /**
     * @brief Static MySQL prepared statement.
     * Static prepared statements need to specify parameter and result bindings during creation,
     * values of them are stored internally.
     * Please note that just because your query has parameters, you do not neccessarily need
     * parameter bindings, you can often hardcode or concatenate them into query string.
     * Two advantages compared to using SuperiorMySqlpp::Query are binary communication with server
     * and stronger type safety guarantees.
     * Created by constructor or Connection::makePreparedStatement.
     * @tparam ResultBindings Specialization of #ResultBindings.
     * @tparam ParamBindings Specialization of #ParamBindings.
     * @tparam storeResult If true, fetches results in "store mode", whole result set in one operation.
     *                     If false, fetches results in "use mode", incrementally.
     *                     Both modes offers some additional unique methods.
     * @tparam validateMode Defines conversion class, i.e. how strictly must match type
     *                      provided in result/param storage and the one from table metadata.
     *                      If stronger class of conversion is required than the one performed, it leads to error.
     * @tparam warnMode Defines level of conversion that triggers warning, but is still acceptable.
     * @tparam ignoreNullable If true, one can store nullable data of type T from database to result storage of type T (directly withnout SuperiorMySqlpp::Nullable).
     *                      If the nullable is in null state, nothing will be assigned to given result field.
     */
    template<typename ResultBindings, typename ParamBindings, bool storeResult, ValidateMetadataMode validateMode, ValidateMetadataMode warnMode, bool ignoreNullable>
    class PreparedStatement : public detail::PreparedStatementBase<storeResult, validateMode, warnMode, ignoreNullable>
    {
    private:
        ResultBindings resultBindings;
        static_assert(!ResultBindings::isParamBinding, "Result bindings are not of type ResultBindings<Args...>!");

        ParamBindings paramsBindings;
        static_assert(ParamBindings::isParamBinding, "Param bindings are not of type ParamBindings<Args...>!");

        bool hasValidatedResultMetadata = false;

    private:
        /**
         * @brief Constructor for PreparedStatement.
         * This overload is designated by SuperiorMySqlpp::fullInitTag and specifies both parameters and results as tuples.
         * @remark It is used internally as implementation of matching public constructor.
         * @param driver LowLevel::DBDriver wrapping MySql connection.
         * @param query String containing query to be executed.
         * @param tag Tag field for specifying this overload.
         * @param resultArgsTuple Forwarded std::tuple containing initial values of resultBindings.
         * @param paramArgsTuple Forwarded std::tuple containing initial values of paramBindings.
         */
        template<typename ResultArgsTuple, typename ParamArgsTuple, std::size_t... RI, std::size_t... PI>
        PreparedStatement(LowLevel::DBDriver& driver,
                          const std::string& query,
                          FullInitTag,
                          ResultArgsTuple&& resultArgsTuple,
                          std::index_sequence<RI...>,
                          ParamArgsTuple&& paramArgsTuple,
                          std::index_sequence<PI...>)
            : detail::PreparedStatementBase<storeResult, validateMode, warnMode, ignoreNullable>{driver.makeStatement()},
              resultBindings{std::get<RI>(std::forward<ResultArgsTuple>(resultArgsTuple))...},
              paramsBindings{std::get<PI>(std::forward<ParamArgsTuple>(paramArgsTuple))...}
        {
            this->statement.prepare(query);

            auto paramCount = this->statement.paramCount();
            if (paramCount != paramsBindings.kArgumentsCount)
            {
                throw PreparedStatementTypeError{"Params count (" + std::to_string(paramCount) +
                        ") in query doesn't match number of arguments (" + std::to_string(paramsBindings.kArgumentsCount) + ")!"};
            }

            if (decltype(paramsBindings)::kArgumentsCount > 0)
            {
                this->statement.bindParam(paramsBindings.bindings.data());
            }
        }

    public:
        /**
         * @brief Constructor for PreparedStatement.
         * This overload is designated by SuperiorMySqlpp::fullInitTag and specifies both parameters and results as tuples.
         * @param driver DBDriver wrapping MySql connection.
         * @param query String containing query to be executed.
         * @param tag Tag field for specifying this overload.
         * @param resultArgsTuple Forwarded std::tuple containing initial values of resultBindings.
         * @param paramArgsTuple Forwarded std::tuple containing initial values of paramBindings.
         */
        template<typename... RArgs, typename... PArgs, template<typename...> class ResultArgsTuple, template<typename...> class ParamArgsTuple>
        PreparedStatement(LowLevel::DBDriver& driver,
                          const std::string& query,
                          FullInitTag tag,
                          ResultArgsTuple<RArgs...>&& resultArgsTuple,
                          ParamArgsTuple<PArgs...>&& paramArgsTuple)
            : PreparedStatement
              {
                  driver, query, tag,
                  std::forward<ResultArgsTuple<RArgs...>>(resultArgsTuple),
                  std::make_index_sequence<sizeof...(RArgs)>{},
                  std::forward<ParamArgsTuple<PArgs...>>(paramArgsTuple),
                  std::make_index_sequence<sizeof...(PArgs)>{}
              }
        {
        }

        /**
         * @brief Constructor for PreparedStatement.
         * This overload is for useful when only DBDriver and (parametrized) query are needed.
         * @param driver DBDriver wrapping MySql connection.
         * @param query String containing query to be executed.
         * @param params Forwarded varargs used for initialization of paramBindings.
         */
        template<typename... Args>
        PreparedStatement(LowLevel::DBDriver& driver, const std::string& query, Args&&... params)
            : PreparedStatement
              {
                  driver, query, fullInitTag,
                  std::forward_as_tuple(),
                  std::forward_as_tuple(std::forward<Args>(params)...)
              }
        {
            InitializeResult(resultBindings.data);
        }

        /**
         * @brief Constructor for PreparedStatement.
         * This overload is useful when #Connection is available.
         * Uses fullInitTag and specifies both parameters and results as tuples.
         * @param connection Reference to opened MySql connection.
         * @param query String containing query to be executed.
         * @param tag Tag field for specifying this overload.
         * @param resultArgsTuple Forwarded std::tuple containing initial values of resultBindings.
         * @param paramArgsTuple Forwarded std::tuple containing initial values of paramBindings.
         */
        template<typename ResultArgsTuple, typename ParamArgsTuple>
        PreparedStatement(Connection& connection,
                          const std::string& query,
                          FullInitTag tag,
                          ResultArgsTuple&& resultArgsTuple,
                          ParamArgsTuple&& paramArgsTuple)
            : PreparedStatement
              {
                  connection.detail_getDriver(), query, tag,
                  std::forward<ResultArgsTuple>(resultArgsTuple),
                  std::forward<ParamArgsTuple>(paramArgsTuple)
              }
        {
        }

        /**
         * @brief Constructor for PreparedStatement.
         * This overload is useful for common case of having
         * Connection and (parametrizable) query along with parameters.
         * @param driver DBDriver wrapping MySql connection.
         * @param query String containing query to be executed.
         * @param params Forwarded varargs used for initialization of paramBindings.
         */
        template<typename... Args>
        PreparedStatement(Connection& connection, const std::string& query, Args&&... params)
            : PreparedStatement
              {
                  connection, query, fullInitTag,
                  std::forward_as_tuple(),
                  std::forward_as_tuple(std::forward<Args>(params)...)
              }
        {
            InitializeResult(resultBindings.data);
        }

        PreparedStatement(const PreparedStatement&) = default;
        PreparedStatement(PreparedStatement&&) = default;
        PreparedStatement& operator=(const PreparedStatement&) = default;
        PreparedStatement& operator=(PreparedStatement&&) = default;

        ~PreparedStatement() = default;

        /**
         * This function updates the binding information of query parameters.
         * Call this function before execute if the values of parameters changed after construction.
         * It works in two phases - first it updates the binding structures to reflect on current
         * values (and calls some unique initialization for given type, if exist), then it updates
         * the bindings in underlying C client. This step is crucial, as some versions apparently
         * may keep own internal copy of binding structures.
         * @remark In theory, shall only be required when some parameter size or memory address has changed since creation.
         *         In practice, it is needed for non-POD types - {@link SuperiorMySqlpp::ArrayBase Array} based types change
         *         their size and {@link SuperiorMySqlpp::Nullable Nullables} require binding initialization.
         */
        void updateParamsBindings()
        {
            if (decltype(paramsBindings)::kArgumentsCount > 0)
            {
                paramsBindings.update();
                this->statement.bindParam(paramsBindings.bindings.data());
            }
        }

        /**
         * Accessor of tuple containing storage of param bindings.
         * Mutable variant.
         */
        auto& getParams() noexcept
        {
            return paramsBindings.data;
        }

        /**
         * Accessor of tuple containing storage of param bindings.
         * Const variant.
         */
        const auto& getParams() const noexcept
        {
            return paramsBindings.data;
        }

        /**
         * Accessor of tuple containing storage of result bindings.
         * Mutable variant.
         */
        auto& getResult() noexcept
        {
            return resultBindings.data;
        }

        /**
         * Accessor of tuple containing storage of result bindings.
         * Const variant.
         */
        const auto& getResult() const noexcept
        {
            return resultBindings.data;
        }

        /**
         * Convenience function to replace values of query parameters.
         * @param args Forwarded new data to replace currenly stored values used for query parameters.
         * @remark You usually want to call #updateParamsBindings afterwards, see that function for explanation.
         */
        template<typename... Args>
        void setParams(Args&&... args)
        {
            paramsBindings.data = std::forward_as_tuple(std::forward<Args>(args)...);

        }

        /**
         * Returns how many bindings exist in parameterBindings.
         */
        static constexpr auto getParamsCount() noexcept
        {
            return decltype(paramsBindings)::kArgumentsCount;
        }

        /**
         * Returns how many bindings exist in resultBindings.
         */
        static constexpr auto getResultCount() noexcept
        {
            return decltype(resultBindings)::kArgumentsCount;
        }

        /**
         * @brief Executes prepared statement.
         * This method starts by executing the query (actually performing it), then it
         * validates metadata for its result (if any). Finally, it calls #PreparedStatement's equivalent of
         * SuperiorMySqlpp::Query::use() or SuperiorMySqlpp::Query::store(), depending on #storeResult flag.
         */
        void execute()
        {
            this->statement.execute();

            auto fieldCount = this->statement.fieldCount();
            if (fieldCount != resultBindings.kArgumentsCount)
            {
                throw PreparedStatementTypeError{"Result types count (" + std::to_string(resultBindings.kArgumentsCount) +
                        ") in query doesn't match number of returned fields (" + std::to_string(fieldCount) + ")!"};
            }

            if (decltype(resultBindings)::kArgumentsCount > 0)
            {
                if (!hasValidatedResultMetadata)
                {
                    this->validateResultMetadata(resultBindings.bindings);
                    this->hasValidatedResultMetadata = true;
                }

                // TODO: check if this might be cached when calling execute multiple times
                this->statement.bindResult(resultBindings.bindings.data());
            }

            this->storeOrUse();
        }
    };
}

