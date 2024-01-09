/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <array>
#include <cmath>
#include <iterator>


namespace SuperiorMySqlpp
{
    struct FirstTag {};
    constexpr FirstTag firstTag{};
    struct SecondTag {};
    constexpr SecondTag secondTag{};

    template<typename FirstIterator, typename SecondIterator>
    class ConcatIterator : public std::iterator<std::bidirectional_iterator_tag, std::decay_t<decltype(*std::declval<FirstIterator>())>>
    {
    private:
        bool first;
        FirstIterator firstBegin;
        FirstIterator firstEnd;
        SecondIterator secondBegin;
        SecondIterator secondEnd;
        union
        {
            FirstIterator firstIterator;
            SecondIterator secondIterator;
        };

    public:
        ConcatIterator(FirstIterator firstBegin, FirstIterator firstEnd,
                       SecondIterator secondBegin, SecondIterator secondEnd,
                       FirstTag, FirstIterator iterator)
            : first{true},
              firstBegin{firstBegin},
              firstEnd{firstEnd},
              secondBegin{secondBegin},
              secondEnd{secondEnd},
              firstIterator{iterator}
        {}

        ConcatIterator(FirstIterator firstBegin, FirstIterator firstEnd,
                       SecondIterator secondBegin, SecondIterator secondEnd,
                       SecondTag, SecondIterator iterator)
            : first{false},
              firstBegin{firstBegin},
              firstEnd{firstEnd},
              secondBegin{secondBegin},
              secondEnd{secondEnd},
              secondIterator{iterator}
        {}

        ~ConcatIterator()
        {
            if (first)
            {
                firstIterator.~FirstIterator();
            }
            else
            {
                secondIterator.~SecondIterator();
            }
        }

        ConcatIterator& operator++()
        {
            if (first)
            {
                if (!(++firstIterator < firstEnd))
                {
                    firstIterator.~FirstIterator();
                    ::new (std::addressof(secondIterator)) SecondIterator{secondBegin};
                    first = false;
                }
            }
            else
            {
                ++secondIterator;
            }

            return *this;
        }

        auto& operator*() const
        {
            if (first)
            {
                return *firstIterator;
            }
            else
            {
                return *secondIterator;
            }
        }

        bool operator==(const ConcatIterator& other) const
        {
            if (first == other.first)
            {
                if (first)
                {
                    return firstIterator == other.firstIterator;
                }
                else
                {
                    return secondIterator == other.secondIterator;
                }
            }
            else
            {
                if (first)
                {
                    if (firstBegin==firstEnd)
                    {
                        return other.secondIterator == other.secondBegin;
                    }
                    else if (other.secondBegin==other.secondEnd)
                    {
                        return firstBegin!=firstEnd && firstIterator==other.firstEnd;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    if (other.firstBegin==other.firstEnd)
                    {
                        return secondIterator == secondBegin;
                    }
                    else if (secondBegin==secondEnd)
                    {
                        return other.firstBegin!=other.firstEnd && other.firstIterator==firstEnd;
                    }
                    else
                    {
                        return false;
                    }
                }

                return false;
            }
        }

        bool operator!=(const ConcatIterator& other) const
        {
            return !(*this == other);
        }
    };

    template<typename FirstIterator, typename SecondIterator, typename Tag, typename Iterator>
    ConcatIterator<std::decay_t<FirstIterator>, std::decay_t<SecondIterator>> makeConcatIterator(
            FirstIterator firstBegin, FirstIterator firstEnd,
            SecondIterator secondBegin, SecondIterator secondEnd,
            Tag&& tag, Iterator iterator)
    {
        return {firstBegin, firstEnd, secondBegin, secondEnd, std::forward<Tag>(tag), iterator};
    }


    template<typename I, typename J>
    std::size_t distance(I begin, J end)
    {
        std::size_t distance = 0;

        for (; begin!=end; ++begin)
        {
            ++distance;
        }

        return distance;
    }
}


