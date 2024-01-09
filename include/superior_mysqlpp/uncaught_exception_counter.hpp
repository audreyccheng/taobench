/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <cxxabi.h>

namespace SuperiorMySqlpp
{
    inline int uncaughtExceptions() noexcept
    {
        auto* globalsPtr = __cxxabiv1::__cxa_get_globals();
        auto* globalsEreasedPtr = reinterpret_cast<char*>(globalsPtr);
        auto* uncaughtExceptionsErasedPtr = globalsEreasedPtr + sizeof(void*);
        auto* uncaughtExceptionsPtr = reinterpret_cast<int*>(uncaughtExceptionsErasedPtr);
        return *uncaughtExceptionsPtr;
    }

    class UncaughtExceptionCounter
    {
    private:
        int exceptionCount;

    public:
        UncaughtExceptionCounter() noexcept
            : exceptionCount{uncaughtExceptions()}
        {}

        bool isNewUncaughtException() const noexcept
        {
            return uncaughtExceptions() > exceptionCount;
        }
    };
}
