#pragma once

#include <array>
#include <stdexcept>

namespace qc
{
    template <typename T, size_t n>
    class CycleArray
    {
        static_assert(n > 0);

        template <bool constant>
        class Iterator
        {
            public: //----------------------------------------------------------

            using iterator_category = std::forward_iterator_tag;
            using value_type = std::conditional_t<constant, const T, T>;
            using difference_type = ptrdiff_t;
            using pointer = value_type *;
            using reference = value_type &;

            constexpr Iterator(const Iterator & other) noexcept = default;
            constexpr explicit Iterator(const Iterator<false> & other) noexcept requires constant :
                _values(other._values),
                _currentIndex(other._currentIndex),
                _relativeIndex(other._relativeIndex)
            {}
            constexpr Iterator(Iterator && other) noexcept = default;

            Iterator & operator=(const Iterator & other) noexcept = default;
            Iterator & operator=(Iterator && other) noexcept = default;

            ~Iterator() noexcept = default;

            value_type & operator*() const noexcept {
                return _values[_currentIndex];
            }

            value_type * operator->() const noexcept {
                return _values + _currentIndex;
            }

            Iterator & operator++() noexcept {
                ++_currentIndex;
                if (_currentIndex == n) {
                    _currentIndex = 0u;
                }
                ++_relativeIndex;
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator temp(*this);
                operator++();
                return temp;
            }

            template <bool constant_>
            bool operator==(const Iterator<constant_> & other) const noexcept {
                return _values == other.values && _relativeIndex == other._relativeIndex;
            }

            private: //---------------------------------------------------------

            constexpr Iterator(value_type * const values, const size_t currentIndex, const size_t relativeIndex) noexcept :
                _values(values),
                _currentIndex(currentIndex),
                _relativeIndex(relativeIndex)
            {}

            value_type * _values;
            size_t _currentIndex;
            size_t _relativeIndex;
        };

        template <bool constant> friend class Iterator;

        public: //--------------------------------------------------------------

        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type &;
        using const_reference = const value_type &;
        using pointer = value_type *;
        using const_pointer = const value_type *;
        using iterator = Iterator<false>;
        using const_iterator = Iterator<true>;

        static_assert(std::is_nothrow_default_constructible_v<value_type>);
        static_assert(std::is_nothrow_move_constructible_v<value_type>);
        static_assert(std::is_nothrow_move_assignable_v<value_type>);
        static_assert(std::is_nothrow_swappable_v<value_type>);
        static_assert(std::is_nothrow_destructible_v<value_type>);

        constexpr CycleArray() noexcept = default;

        constexpr CycleArray(const CycleArray & other) noexcept :
            _frontIndex(other._frontIndex),
            _values(other._values)
        {}

        constexpr CycleArray(CycleArray && other) noexcept :
            _frontIndex(other._frontIndex),
            _values(std::move(other._values))
        {
            other.startIndex = 0u;
        }

        CycleArray & operator=(const CycleArray & other) noexcept
        {
            _frontIndex = other._frontIndex;
            _values = other._values;

            return *this;
        }

        CycleArray & operator=(CycleArray && other) noexcept
        {
            _frontIndex = other._frontIndex;
            _values = std::move(other._values);

            other._frontIndex = 0u;

            return *this;
        }

        ~CycleArray() noexcept = default;

        constexpr void swap(CycleArray & other) noexcept
        {
            std::swap(_values, other._values);
            std::swap(_frontIndex, other._frontIndex);
        }

        void fill(const T & v)
        {
            _values.fill(v);
        }

        T push(const T & v)
        {
            return _push(v);
        }

        T push(T && v) noexcept
        {
            return _push(std::move(v));
        }

        constexpr size_t size() const noexcept
        {
            return n;
        }

        constexpr bool empty() const noexcept
        {
            return false;
        }

        constexpr value_type & front() noexcept
        {
            return const_cast<value_type &>(const_cast<const CycleArray &>(*this).front());
        }

        constexpr const value_type & front() const noexcept
        {
            return _values[_frontIndex];
        }

        constexpr value_type & back() noexcept
        {
            return const_cast<value_type &>(const_cast<const CycleArray &>(*this).back());
        }

        constexpr const value_type & back() const noexcept
        {
            return _values[(_frontIndex == 0u ? n : _frontIndex) - 1u];
        }

        constexpr value_type & operator[](const size_t i)
        {
            return const_cast<value_type &>(const_cast<const CycleArray &>(*this).operator[](i));
        }

        constexpr const value_type & operator[](const size_t i) const
        {
            const size_t absoluteIndex{_frontIndex + i};
            return _values[absoluteIndex < n ? absoluteIndex : absoluteIndex - n];
        }

        constexpr value_type & at(const size_t i)
        {
            return const_cast<value_type &>(const_cast<const CycleArray &>(*this).at(i));
        }

        constexpr const value_type & at(const size_t i) const
        {
            if (i >= n) {
                throw std::out_of_range("Index out of bounds");
            }

            return operator[](i);
        }

        constexpr iterator begin() noexcept
        {
            return Iterator<false>(_values.data(), _frontIndex, 0);
        }

        constexpr const_iterator begin() const noexcept
        {
            return Iterator<true>(_values.data(), _frontIndex, 0);
        }

        constexpr const_iterator cbegin() const noexcept
        {
            return begin();
        }

        constexpr iterator end() noexcept
        {
            return Iterator<false>(_values.data(), _frontIndex, n);
        }

        constexpr const_iterator end() const noexcept
        {
            return Iterator<true>(_values.data(), _frontIndex, n);
        }

        constexpr const_iterator cend() const noexcept
        {
            return end();
        }

        private: //-------------------------------------------------------------

        template <typename T_>
        T _push(T_ && v)
        {
            if (_frontIndex == 0u) {
                _frontIndex = n - 1u;
            }
            else {
                --_frontIndex;
            }

            T * const slot(_values.data() + _frontIndex);

            T prevVal(std::move(*slot));
            slot->~T();

            new (slot) T(std::forward<T_>(v));

            return prevVal;
        }

        size_t _frontIndex{0u};
        std::array<T, n> _values{};
    };

    template <typename T, size_t n>
    bool operator==(const CycleArray<T, n> & arr1, const CycleArray<T, n> & arr2)
    {
        if (&arr1 == &arr2) {
            return true;
        }

        auto it1(arr1.cbegin());
        auto it2(arr2.cbegin());
        const auto end1(arr1.cend());
        const auto end2(arr2.cend());
        for (; it1 != end1 && it2 != end2; ++it1, ++it2) {
            if (*it1 != *it2) {
                return false;
            }
        }

        return true;
    }
}
