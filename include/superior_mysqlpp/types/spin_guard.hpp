/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <atomic>


namespace SuperiorMySqlpp
{
    template<std::memory_order lockOrder=std::memory_order_acquire, std::memory_order unlockOrder=std::memory_order_release>
    class SpinGuardBase
    {
    private:
        std::atomic_flag& lock;

    public:
        SpinGuardBase() = delete;
        SpinGuardBase(const SpinGuardBase&) = delete;
        SpinGuardBase(SpinGuardBase&&) = delete;

        SpinGuardBase(decltype(lock)& lock)
            : lock{lock}
        {
            while (lock.test_and_set(lockOrder)) {}
        }

        ~SpinGuardBase()
        {
            lock.clear(unlockOrder);
        }
    };

    using SpinGuard = SpinGuardBase<>;
}
