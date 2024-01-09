/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <chrono>
#include <thread>
#include <cassert>


namespace SuperiorMySqlpp { namespace detail
{
    /**
     * Conditional sleep with upper bound limit.
     * @param time Maximum time to sleep in total.
     * @param part Sleep interval between periodic #condition checks.
     * @param condition Breaks sleeping when condition's result is not true.
     */
    template<typename T1, typename T2, typename C, typename Clock=std::chrono::high_resolution_clock>
    void sleepInParts(T1&& time, T2&& part, C&& condition)
    {
        assert(time.count() >= 0);
        assert(part.count() >= 0);

        auto sleepStart = Clock::now();
        auto sleepEnd = sleepStart + time;

        while(condition())
        {
            auto remains = std::max(decltype(sleepEnd-Clock::now()){0}, sleepEnd-Clock::now());
            if (remains > part)
            {
                remains = part;
            }

            if (remains.count() > 0)
            {
                std::this_thread::sleep_for(remains);
            }
            else
            {
                break;
            }
        }
    }
}}
