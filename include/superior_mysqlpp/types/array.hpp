/*
 * Author: Tomas Nozicka
 */

#pragma once


#include <array>
#include <cmath>
#include <cstring>


namespace SuperiorMySqlpp
{
    /**
     * Base class for all array based SQL types
     *
     * If TerminatingZero == true, hidden empty byte is always appended after payload.
     * Mind the fact that storage needs to be (at least one byte) larger for this mode.
     */
    template<std::size_t N, bool TerminatingZero = false>
    class ArrayBase
    {
    protected:
        std::array<char, N + int(TerminatingZero)> array;

        using ItemsCount_t = std::size_t;
        ItemsCount_t itemsCount = 0;

    public:
        /**
         * Note: should be constexpr, sadly GCC 4.9.2 from Debian Jessie doesn't implement C++14 sufficiently
         */
        ArrayBase()
        {
            if (TerminatingZero)
            {
                array.front() = '\0';
                // back() is just redundant sanity check
                array.back() = '\0';
            }
        }

        ArrayBase(const ArrayBase& other)
        {
            assign(other);
        }

        ArrayBase(ArrayBase&& other)
        {
            assign(other);
        }

        ArrayBase& operator=(const ArrayBase& other)
        {
            assign(other);
            return *this;
        }

        ArrayBase& operator=(ArrayBase&& other)
        {
            assign(other);
            return *this;
        }

        ~ArrayBase() = default;

    private:
        constexpr ItemsCount_t count() const
        {
            return std::min(itemsCount, static_cast<ItemsCount_t>(N));
        }

        template<std::size_t NN = N>
        std::enable_if_t<(NN > 0),void> assign(const ArrayBase& other)
        {
            std::memcpy(array.data(), other.array.data(), other.size());
            itemsCount = other.size();
            if (TerminatingZero)
            {
                *this->end() = '\0';
            }
        }

        template<std::size_t NN = N>
        std::enable_if_t<(NN == 0),void> assign(const ArrayBase&) {}

    public:
        ItemsCount_t& counterRef()
        {
            return itemsCount;
        }

        constexpr const ItemsCount_t& counterRef() const
        {
            return itemsCount;
        }

        auto begin()
        {
            return array.begin();
        }

        auto begin() const
        {
            return array.begin();
        }

        auto cbegin() const
        {
            return array.cbegin();
        }

        auto end()
        {
            return begin() + count();
        }

        auto end() const
        {
            return cbegin() + count();
        }

        auto cend() const
        {
            return cbegin() + count();
        }

        auto endOfStorage()
        {
            return array.end();
        }

        auto endOfStorage() const
        {
            return array.end();
        }

        auto cendOfStorage() const
        {
            return array.cend();
        }

        auto data()
        {
            return array.data();
        }

        constexpr auto data() const
        {
            return array.data();
        }

        auto& front()
        {
            if (empty())
                throw std::out_of_range{"SuperiorMySqlpp::ArrayBase::front"};
            else
                return array.front();
        }

        constexpr const auto& front() const
        {
            if (empty())
                throw std::out_of_range{"SuperiorMySqlpp::ArrayBase::front"};
            else
                return array.front();
        }

        auto& back()
        {
            if (empty())
                throw std::out_of_range{"SuperiorMySqlpp::ArrayBase::back"};
            else
                return *(array.begin() + count() - 1);
        }

        constexpr auto& back() const
        {
            if (empty())
                throw std::out_of_range{"SuperiorMySqlpp::ArrayBase::back"};
            else
                return *(array.begin() + count() - 1);
        }

        auto& operator[](std::size_t position)
        {
            return *(begin()+position);
        }

        constexpr const auto& operator[](std::size_t position) const
        {
            return *(begin()+position);
        }

        constexpr auto size() const
        {
            return count();
        }

        constexpr auto maxSize() const
        {
            return N;
        }

        bool empty() const
        {
            return count() == 0;
        }
    };
}


