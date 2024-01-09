#pragma once

namespace SuperiorMySqlpp
{
    namespace detail
    {
        /**
         * @brief Wraps member object and member function pointer into single functor
         * @tparam T Wrapped class type
         * @tparam TMember Type of member function
         * @tparam Args Arguments of TMember function
         */
        template<typename T, typename TMember, typename... Args>
        struct MemberWrapper
        {
            T *object;
            TMember member;

            /**
             * @brief Constructs MemberWrapper object
             * @param object Instance of T, note: pointer is not owned -> possible out of scope error
             * @param member Member function from T
             */
            constexpr MemberWrapper(T *object, TMember member) noexcept
                : object(object), member(member) {}

            /**
             * @brief Invokes stored member function with args
             * @param args Parameter pack of function arguments
             */
            inline void operator()(Args... args) const
            {
                (object->*member)(args...);
            }
        };
    }

    /**
     * @brief Constructs MemberWrapper object from pointer to T and member function
     *        returning void
     * @param object pointer to T
     * @param member function from T
     */
    template<typename T, typename... Args>
    inline auto wrapMember(T *object, void (T::*member)(Args...)) noexcept
    {
        return detail::MemberWrapper<T, decltype(member), Args...>(object, member);
    }

    /**
     * @brief Constructs MemberWrapper object from pointer to volatile T and volatile
     *        member function returning void
     * @param object pointer to volatile T
     * @param member volatile function from T
     */
    template<typename T, typename... Args>
    inline auto wrapMember(volatile T *object, void (T::*member)(Args...) volatile) noexcept
    {
        return detail::MemberWrapper<volatile T, decltype(member), Args...>(object, member);
    }

    /**
     * @brief Constructs MemberWrapper object from pointer to const T and const member
     *        function returning void
     * @param object pointer to const T
     * @param member const function from T
     */
    template<typename T, typename... Args>
    inline auto wrapMember(const T *object, void (T::*member)(Args...) const) noexcept
    {
        return detail::MemberWrapper<const T, decltype(member), Args...>(object, member);
    }

    /**
     * @brief Constructs MemberWrapper object from pointer to const-volatile T and
     *        const-volatile member function returning void
     * @param object pointer to const-volatile T
     * @param member const-volatile function from T
     */
    template<typename T, typename... Args>
    inline auto wrapMember(const volatile T *object, void (T::*member)(Args...) const volatile) noexcept
    {
        return detail::MemberWrapper<const volatile T, decltype(member), Args...>(object, member);
    }
}
