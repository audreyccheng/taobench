/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <utility>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <superior_mysqlpp/prepared_statements/default_initialize_result.hpp>
#include <superior_mysqlpp/prepared_statements/binding_types.hpp>
#include <superior_mysqlpp/exceptions.hpp>



namespace SuperiorMySqlpp
{

class BadNullableAccess : public LogicError
{
public:
  using LogicError::LogicError;

  virtual ~BadNullableAccess() noexcept = default;
};


struct DisengagedOption
{
    // Do not user-declare default constructor at all for optional_value = {} syntax to work.
    // DisengagedOption() = delete;

    // Used for constructing DisengagedOption.
    enum class Construct
    {
        Token
    };

    // Must be constexpr for DisengagedOption to be literal.
    explicit constexpr DisengagedOption(Construct)
    {
    }
};
constexpr DisengagedOption disengagedOption{DisengagedOption::Construct::Token};
constexpr DisengagedOption null{DisengagedOption::Construct::Token};


struct InPlace
{
};
constexpr InPlace inPlace{};

namespace detail {
    class NullableBase {
    protected:
        bool engaged;
        bool null;

        constexpr NullableBase()
            : engaged(false), null(true)
        {
        }

        constexpr NullableBase(bool engaged, bool null)
            : engaged(engaged), null(null)
        {
        }

    public:
        void engage()
        {
            engaged = true;
        }

        bool isNull()
        {
            return null;
        }
    };
}

template<typename T>
class Nullable : public detail::NullableBase
{
public:
    using StoredType = std::remove_const_t<T>;

protected:
    struct Empty {};
    union
    {
        Empty empty;
        StoredType payload;
    };

protected:
    void destroyPayload() noexcept(std::is_nothrow_destructible<StoredType>())
    {
        if (!std::is_trivially_destructible<StoredType>())
        {
            payload.~StoredType();
        }
        engaged = false;
    }

    void destroyPayloadIfEngaged() noexcept(std::is_nothrow_destructible<StoredType>())
    {
        if (!std::is_trivially_destructible<StoredType>())
        {
            if (engaged)
            {
                payload.~StoredType();
                engaged = false;
            }
        }
    }

    template<typename... Args>
    void constructPayload(Args&&... args) noexcept(std::is_nothrow_constructible<StoredType, Args...>())
    {
        ::new (std::addressof(payload)) StoredType{std::forward<Args>(args)...};
        engaged = true;
    }

public:
    /*
     * Member functions
     */
    constexpr Nullable() noexcept
        : empty{}
    {
    }

    constexpr Nullable(DisengagedOption) noexcept
        : Nullable{}
    {
    }

    Nullable(const Nullable& other)
        : NullableBase(other.engaged, other.null)
    {
        if (other.engaged)
        {
            constructPayload(other.payload);
        }
    }

    Nullable(Nullable&& other)
        : NullableBase(other.engaged, other.null)
    {
        if (other.engaged)
        {
            constructPayload(std::move(other.payload));
        }
    }

    constexpr Nullable(const StoredType& value)
        : NullableBase(true, false), payload{value}
    {
    }

    constexpr Nullable(StoredType&& value) noexcept(std::is_nothrow_move_constructible<StoredType>())
        : NullableBase(true, false), payload{std::move(value)}
    {
    }

    Nullable& operator=(DisengagedOption) noexcept(noexcept(std::declval<Nullable>().destroyPayload()))
    {
        destroyPayloadIfEngaged();
        null = true;
        return *this;
    }

    template<typename... Args>
    constexpr explicit Nullable(InPlace, Args&&... args)
        : NullableBase(true, false), payload{std::forward<Args>(args)...}
    {
    }

    template<typename U, typename... Args,
             std::enable_if_t<std::is_constructible<StoredType, std::initializer_list<U>&, Args&&...>::value, int>...>
    constexpr explicit Nullable(InPlace, std::initializer_list<U> initializerList, Args&&... args)
        : NullableBase(true, false), payload{initializerList, std::forward<Args>(args)...}
    {
    }


    Nullable& operator=(const Nullable& other)
    {
        if (engaged && other.engaged)
        {
            payload = other.payload;
            null = other.null;
        }
        else
        {
            if (other.engaged)
            {
                constructPayload(other.payload);
                null = other.null;
            }
            else
            {
                destroyPayload();
                null = true;
            }
        }

        return *this;
    }

    Nullable& operator=(Nullable&& other) noexcept(std::is_nothrow_move_constructible<StoredType>() && std::is_nothrow_move_assignable<StoredType>())
    {
        if (engaged && other.engaged)
        {
            payload = std::move(other.payload);
            null = other.null;
        }
        else
        {
            if (other.engaged)
            {
                constructPayload(std::move(other.payload));
                null = other.null;
            }
            else
            {
                destroyPayload();
                null = true;
            }
        }

        return *this;
    }

    template<typename U>
    std::enable_if_t<std::is_same<StoredType, std::decay_t<U>>::value, Nullable&> operator=(U&& value)
    {
        static_assert(std::is_constructible<StoredType, U>() && std::is_assignable<StoredType&, U>(), "Cannot assign to value type from argument");

        if (engaged)
        {
            payload = std::forward<U>(value);
        }
        else
        {
            constructPayload(std::forward<U>(value));
        }

        null = false;

        return *this;
    }

    ~Nullable()
    {
        if (!std::is_trivially_destructible<StoredType>())
        {
            if (engaged)
            {
                payload.~StoredType();
            }
        }
    }


    /*
     * Observers
     */
    constexpr bool isValid() const
    {
        return engaged && !null;
    }

    constexpr const StoredType* operator->() const noexcept
    {
        return reinterpret_cast<const StoredType*>(&empty);
    }

    StoredType* operator->() noexcept
    {
        return reinterpret_cast<StoredType*>(&empty);
    }

    constexpr const StoredType& operator*() const noexcept
    {
        return payload;
    }

    StoredType& operator*() noexcept
    {
        return payload;
    }

    constexpr explicit operator bool() const noexcept
    {
        return isValid();
    }

    /* constexpr */ const StoredType& value() const
    {
        if (!isValid()) {
            throw BadNullableAccess{"Attempt to access value of a invalid nullable object!"};
        }
        return payload;
    }

    StoredType& value()
    {
        if (isValid())
        {
            return payload;
        }
        else
        {
            throw BadNullableAccess{"Attempt to access value of a invalid nullable object!"};
        }
    }

    template<typename U>
    constexpr StoredType valueOr(U&& value) const &
    {
        static_assert(std::is_copy_constructible<StoredType>() && std::is_convertible<U&&, StoredType>(), "Cannot return value");

        return isValid()? payload : StoredType{std::forward<U>(value)};
    }

    template<typename U>
    StoredType value_or(U&& value) &&
    {
        static_assert(std::is_move_constructible<StoredType>() && std::is_convertible<U&&, StoredType>(), "Cannot return value" );

        return isValid()? std::move(payload) : StoredType{std::forward<U>(value)};
    }


    /*
     * Modifiers
     */
    void disengage() noexcept(noexcept(destroyPayloadIfEngaged()))
    {
        destroyPayloadIfEngaged();
        null = false;
    }

    void unsetNull() noexcept
    {
        null = false;
    }

    void setNull() noexcept
    {
        if (engaged)
        {
            null = true;
        }
    }

    void swap(Nullable& other) noexcept(std::is_nothrow_move_constructible<StoredType>() && noexcept(swap(std::declval<StoredType&>(), std::declval<StoredType&>())))
    {
        if (engaged && other.engaged)
        {
            ::std::swap(payload, other.payload);
            ::std::swap(null, other.null);
        }
        else if (engaged)
        {
            other.constructPayload(std::move(payload));
            destroyPayload();
            other.null = null;
            null = false;
        }
        else
        {
            constructPayload(std::move(other.payload));
            other.destroyPayload();
            null = other.null;
            other.null = false;
        }
    }

    template<typename... Args>
    void emplace(Args&&... args)
    {
        static_assert(std::is_constructible<StoredType, Args&&...>(), "Cannot emplace value type from arguments");

        destroyPayloadIfEngaged();
        null = false;
        constructPayload(std::forward<Args>(args)...);
    }

    template<typename U, typename... Args>
    std::enable_if_t<std::is_constructible<StoredType, std::initializer_list<U>&, Args&&...>::value, void>
    emplace(std::initializer_list<U> initializerList, Args&&... args)
    {
        destroyPayloadIfEngaged();
        null = false;
        constructPayload(initializerList, std::forward<Args>(args)...);
    }



    /*
     * DO NOT USE this function unless you really need to!
     */
    StoredType& detail_getPayloadRef()
    {
        return payload;
    }

    /*
     * DO NOT USE this function unless you really need to!
     */
    bool& detail_getNullRef()
    {
        return null;
    }
};


/*
 * Comparisons between optional objects
 */
template<typename T>
constexpr bool operator==(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return static_cast<bool>(lhs) == static_cast<bool>(rhs) && (!lhs || *lhs == *rhs);
}

template<typename T>
constexpr bool operator!=(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
constexpr bool operator<(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return static_cast<bool>(rhs) && (!lhs || *lhs < *rhs);
}

template<typename T>
constexpr bool operator>(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return rhs < lhs;
}

template<typename T>
constexpr bool operator<=(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return !(rhs < lhs);
}

template<typename T>
constexpr bool operator>=(const Nullable<T>& lhs, const Nullable<T>& rhs)
{
    return !(lhs < rhs);
}

/*
 * Comparisons with Nullopt
 */
template<typename T>
constexpr bool operator==(const Nullable<T>& lhs, DisengagedOption) noexcept
{
    return !lhs;
}

template<typename T>
constexpr bool operator==(DisengagedOption, const Nullable<T>& rhs) noexcept
{
    return !rhs;
}

template<typename T>
constexpr bool operator!=(const Nullable<T>& lhs, DisengagedOption) noexcept
{
    return static_cast<bool>(lhs);
}

template<typename T>
constexpr bool operator!=(DisengagedOption, const Nullable<T>& rhs) noexcept
{
    return static_cast<bool>(rhs);
}

template<typename T>
constexpr bool operator<(const Nullable<T>&, DisengagedOption) noexcept
{
    return false;
}

template<typename T>
constexpr bool operator<(DisengagedOption, const Nullable<T>& rhs) noexcept
{
    return static_cast<bool>(rhs);
}

template<typename T>
constexpr bool operator>(const Nullable<T>& lhs, DisengagedOption) noexcept
{
    return static_cast<bool>(lhs);
}

template<typename T>
constexpr bool operator>(DisengagedOption, const Nullable<T>&) noexcept
{
    return false;
}

template<typename T>
constexpr bool operator<=(const Nullable<T>& lhs, DisengagedOption) noexcept
{
    return !lhs;
}

template<typename T>
constexpr bool operator<=(DisengagedOption, const Nullable<T>&) noexcept
{
    return true;
}

template<typename T>
constexpr bool operator>=(const Nullable<T>&, DisengagedOption) noexcept
{
    return true;
}

template<typename T>
constexpr bool operator>=(DisengagedOption, const Nullable<T>& rhs) noexcept
{
    return !rhs;
}

/*
 * Comparisons with value type
 */
template<typename T>
constexpr bool operator==(const Nullable<T>& lhs, const T& rhs)
{
    return lhs && *lhs == rhs;
}

template<typename T>
constexpr bool operator==(const T& lhs, const Nullable<T>& rhs)
{
    return rhs && lhs == *rhs;
}

template<typename T>
constexpr bool operator!=(const Nullable<T>& lhs, T const& rhs)
{
    return !lhs || !(*lhs == rhs);
}

template<typename T>
constexpr bool operator!=(const T& lhs, const Nullable<T>& rhs)
{
    return !rhs || !(lhs == *rhs);
}

template<typename T>
constexpr bool operator<(const Nullable<T>& lhs, const T& rhs)
{
    return !lhs || *lhs < rhs;
}

template<typename T>
constexpr bool operator<(const T& lhs, const Nullable<T>& rhs)
{
    return rhs && lhs < *rhs;
}

template<typename T>
constexpr bool operator>(const Nullable<T>& lhs, const T& rhs)
{
    return lhs && rhs < *lhs;
}

template<typename T>
constexpr bool operator>(const T& lhs, const Nullable<T>& rhs)
{
    return !rhs || *rhs < lhs;
}

template<typename T>
constexpr bool operator<=(const Nullable<T>& lhs, const T& rhs)
{
    return !lhs || !(rhs < *lhs);
}

template<typename T>
constexpr bool operator<=(const T& lhs, const Nullable<T>& rhs)
{
    return rhs && !(*rhs < lhs);
}

template<typename T>
constexpr bool operator>=(const Nullable<T>& lhs, const T& rhs)
{
    return lhs && !(*lhs < rhs);
}

template<typename T>
constexpr bool operator>=(const T& lhs, const Nullable<T>& rhs)
{
    return !rhs || !(lhs < *rhs);
}


/*
 * makeNullable
 */
template<typename T>
constexpr Nullable<std::decay_t<T>> makeNullable(T&& value)
{
    return std::forward<T>(value);
}


namespace detail
{
    template<typename T>
    struct CanBindAsParam<BindingTypes::Nullable, Nullable<T>> : std::true_type {};

    template<typename T>
    struct CanBindAsResult<BindingTypes::Nullable, Nullable<T>> : std::true_type {};
}


/*
 * initializeResultItem
 */
template<typename T>
struct InitializeResultItemImpl<Nullable<T>>
{
    template<typename U>
    static void call(U&& nullable)
    {
        std::forward<U>(nullable) = Nullable<T>{inPlace};
    }
};

} // end of namespace SuperiorMySqlpp


namespace std
{

template<typename T>
inline void swap(SuperiorMySqlpp::Nullable<T>& lhs, SuperiorMySqlpp::Nullable<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

} // end of namespace std
