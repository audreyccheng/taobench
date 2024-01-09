/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <array>
#include <cmath>


namespace SuperiorMySqlpp
{
    template<typename IteratorType, typename GetterType>
    class SpecialIterator
    {
    private:
        IteratorType iterator;
        GetterType getter;

    public:
        template<typename Iterator, typename Getter>
        SpecialIterator(Iterator&& iterator, Getter&& getter)
            : iterator{std::forward<Iterator>(iterator)},
              getter{std::forward<Getter>(getter)}
        {}

        SpecialIterator& operator++()
        {
            ++iterator;
            return *this;
        }

        auto& operator*() const
        {
            return getter(*iterator);
        }

        bool operator==(const SpecialIterator& other) const
        {
            return iterator == other.iterator;
        }

        bool operator!=(const SpecialIterator& other) const
        {
            return iterator != other.iterator;
        }
    };

    template<typename Iterator, typename Getter>
    SpecialIterator<std::decay_t<Iterator>, std::decay_t<Getter>> makeSpecialIterator(Iterator&& iterator, Getter&& getter)
    {
        return {std::forward<Iterator>(iterator), std::forward<Getter>(getter)};
    }
}


