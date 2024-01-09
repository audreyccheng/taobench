/*
 * Author: Tomas Nozicka
 */

#pragma once

#include <utility>


namespace SuperiorMySqlpp { namespace detail
{
    template<typename Pointer>
    class Iterator
    {
    private:
        using ThisClass_t = Iterator<Pointer>;

    protected:
        Pointer pointer;

    public:
        template<typename... Args>
        Iterator(Pointer pointer, Args...)
            : pointer{pointer}
        {
        }

        Iterator(const Iterator&) = default;
        Iterator& operator=(const Iterator&) = default;

        Iterator(Iterator&&) = default;
        Iterator& operator=(Iterator&&) = default;

        ~Iterator() = default;

        auto& operator*() const
        {
            return *pointer;
        }

        ThisClass_t& operator++()
        {
            ++pointer;
            return *this;
        }

        ThisClass_t operator++(int)
        {
            auto tmp(*this);
            operator++();
            return tmp;
        }

        ThisClass_t& operator--()
        {
            --pointer;
            return *this;
        }

        ThisClass_t operator--(int)
        {
            auto tmp(*this);
            operator--();
            return tmp;
        }

        ThisClass_t& operator+=(const ThisClass_t& iterator)
        {
          pointer += iterator.pointer;
          return *this;
        }

        bool operator!=(const ThisClass_t& iterator) const
        {
            return pointer != iterator.pointer;
        }
    };
}}
