#pragma once

#include <stdexcept>
#include <utility>

namespace qc
{
    template <typename T> class HeapArray
    {
        public: //--------------------------------------------------------------

        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using iterator = pointer;
        using const_iterator = const_pointer;

        static_assert(std::is_nothrow_default_constructible_v<T>);
        static_assert(std::is_nothrow_move_constructible_v<T>);
        static_assert(std::is_nothrow_move_assignable_v<T>);
        static_assert(std::is_nothrow_swappable_v<T>);
        static_assert(std::is_nothrow_destructible_v<T>);

        constexpr HeapArray() noexcept = default;
        explicit HeapArray(size_t size) noexcept;
        HeapArray(size_t size, const T & v) noexcept;
        template <typename Iter> HeapArray(Iter first, Iter last);
        HeapArray(T * values, size_t size);

        HeapArray(const HeapArray & other) noexcept;
        constexpr HeapArray(HeapArray && other) noexcept;

        HeapArray & operator=(const HeapArray & other) noexcept;
        HeapArray & operator=(HeapArray && other) noexcept;

        ~HeapArray() noexcept;

        constexpr void swap(HeapArray & other) noexcept;

        void clear() noexcept;

        constexpr size_t size() const noexcept;

        constexpr bool empty() const noexcept;

        constexpr T * data() noexcept;
        constexpr const T * data() const noexcept;

        constexpr T & front() noexcept;
        constexpr const T & front() const noexcept;

        constexpr T & back() noexcept;
        constexpr const T & back() const noexcept;

        constexpr T & operator[](size_t i);
        constexpr const T & operator[](size_t i) const;

        constexpr T & at(size_t i);
        constexpr const T & at(size_t i) const;

        constexpr iterator begin() noexcept;
        constexpr const_iterator begin() const noexcept;
        constexpr const_iterator cbegin() const noexcept;

        constexpr iterator end() noexcept;
        constexpr const_iterator end() const noexcept;
        constexpr const_iterator cend() const noexcept;

        private: //-------------------------------------------------------------

        size_t _size{0u};
        T * _values{nullptr};
    };

    template <typename T> bool operator==(const HeapArray<T> & arr1, const HeapArray<T> & arr2);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace qc
{
    template <typename T>
    inline HeapArray<T>::HeapArray(const size_t size) noexcept :
        _size(size),
        _values(_size ? static_cast<T *>(::operator new(_size * sizeof(T))) : nullptr)
    {
        if constexpr (!std::is_trivially_default_constructible_v<T>) {
            for (size_t i{0u}; i < _size; ++i) {
                new (_values + i) T();
            }
        }
    }

    template <typename T>
    inline HeapArray<T>::HeapArray(const size_t size, const T & v) noexcept :
        _size(size),
        _values(_size ? ::operator new(_size * sizeof(T)) : nullptr)
    {
        for (size_t i{0u}; i < _size; ++i) {
            new (_values + i) T(v);
        }
    }

    template <typename T>
    template <typename Iter>
    inline HeapArray<T>::HeapArray(const Iter first, const Iter last) :
        _size(std::distance(first, last)),
        _values(_size ? static_cast<T *>(::operator new(_size * sizeof(T))) : nullptr)
    {
        if constexpr (std::is_trivially_copy_constructible_v<T>) {
            std::copy_n(first, _size, _values);
        }
        else {
            T * pos{_values};
            for (Iter iter(first); iter != last; ++iter, ++pos) {
                new (pos) T(*iter);
            }
        }
    }

    template <typename T>
    inline HeapArray<T>::HeapArray(T * const values, const size_t size) :
        _size(size),
        _values(values)
    {}

    template <typename T>
    inline HeapArray<T>::HeapArray(const HeapArray & other) noexcept :
        _size(other._size),
        _values(_size ? static_cast<T *>(::operator new(_size * sizeof(T))) : nullptr)
    {
        for (size_t i{0u}; i < _size; ++i) {
            new (_values + i) T(other._values[i]);
        }
    }

    template <typename T>
    inline constexpr HeapArray<T>::HeapArray(HeapArray && other) noexcept :
        _size(std::exchange(other._size, 0u)),
        _values(std::exchange(other._values, nullptr))
    {}

    template <typename T>
    inline HeapArray<T> & HeapArray<T>::operator=(const HeapArray & other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (_size == other._size) {
            for (size_t i{0u}; i < _size; ++i) {
                _values[i] = other._values[i];
            }
        }
        else {
            clear();

            if (other._size) {
                _size = other._size;
                _values = static_cast<T *>(::operator new(_size * sizeof(T)));
                for (size_t i{0u}; i < _size; ++i) {
                    new (_values + i) T(other._values[i]);
                }
            }
        }

        return *this;
    }

    template <typename T>
    inline HeapArray<T> & HeapArray<T>::operator=(HeapArray && other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        clear();

        _size = std::exchange(other._size, 0u);
        _values = std::exchange(other._values, nullptr);

        return *this;
    }

    template <typename T>
    inline HeapArray<T>::~HeapArray() noexcept
    {
        clear();
    }

    template <typename T>
    inline constexpr void HeapArray<T>::swap(HeapArray & other) noexcept
    {
        std::swap(_size, other._size);
        std::swap(_values, other._values);
    }

    template <typename T>
    inline void HeapArray<T>::clear() noexcept
    {
        if (_size) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i{0u}; i < _size; ++i) {
                    _values[i].~T();
                }
            }

            ::operator delete(_values);
            _size = 0u;
            _values = nullptr;
        }
    }

    template <typename T>
    inline constexpr size_t HeapArray<T>::size() const noexcept
    {
        return _size;
    }

    template <typename T>
    inline constexpr bool HeapArray<T>::empty() const noexcept
    {
        return !_size;
    }

    template <typename T>
    inline constexpr T * HeapArray<T>::data() noexcept
    {
        return _values;
    }

    template <typename T>
    inline constexpr const T * HeapArray<T>::data() const noexcept
    {
        return _values;
    }

    template <typename T>
    inline constexpr T & HeapArray<T>::front() noexcept
    {
        return *_values;
    }

    template <typename T>
    inline constexpr const T & HeapArray<T>::front() const noexcept
    {
        return *_values;
    }

    template <typename T>
    inline constexpr T & HeapArray<T>::back() noexcept
    {
        return _values[_size - 1u];
    }

    template <typename T>
    inline constexpr const T & HeapArray<T>::back() const noexcept
    {
        return _values[_size - 1u];
    }

    template <typename T>
    inline constexpr T & HeapArray<T>::operator[](const size_t i)
    {
        return _values[i];
    }

    template <typename T>
    inline constexpr const T & HeapArray<T>::operator[](const size_t i) const
    {
        return _values[i];
    }

    template <typename T>
    inline constexpr T & HeapArray<T>::at(const size_t i)
    {
        return const_cast<T &>(const_cast<const HeapArray &>(*this).at(i));
    }

    template <typename T>
    inline constexpr const T & HeapArray<T>::at(const size_t i) const
    {
        if (i >= _size) {
            throw std::out_of_range("Index out of bounds");
        }

        return operator[](i);
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::begin() noexcept -> iterator
    {
        return _values;
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::begin() const noexcept -> const_iterator
    {
        return _values;
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::cbegin() const noexcept -> const_iterator {
        return _values;
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::end() noexcept -> iterator
    {
        return _values + _size;
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::end() const noexcept -> const_iterator
    {
        return _values + _size;
    }

    template <typename T>
    inline constexpr auto HeapArray<T>::cend() const noexcept -> const_iterator
    {
        return _values + _size;
    }

    template <typename T>
    inline bool operator==(const HeapArray<T> & arr1, const HeapArray<T> & arr2)
    {
        if (&arr1 == &arr2) {
            return true;
        }

        if (arr1.size() != arr2.size()) {
            return false;
        }

        for (size_t i{0u}; i < arr1.size(); ++i) {
            if (arr1[i] != arr2[i]) {
                return false;
            }
        }

        return true;
    }
}
