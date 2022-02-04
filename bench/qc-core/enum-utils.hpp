#pragma once

#include <array>
#include <iterator>
#include <type_traits>

#include <qc-core/core.hpp>

namespace qc
{
    template <typename E>
    concept CountableEnum =
        std::is_enum_v<E> &&
        std::is_unsigned_v<std::underlying_type_t<E>> &&
        requires { E::_n; };

    template <CountableEnum E> constexpr size_t enumCount{size_t(E::_n)};

    // Don't think we need this, but keeping it around for a little while just in case
    #if 0

    template <CountableEnum E>
    struct EnumIterator
    {
        using iterator_category = std::input_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = const E;
        using pointer = const E *;
        using reference = const E &;

        E current{};

        constexpr reference operator*() const noexcept
        {
            return current;
        }

        constexpr pointer operator->() const noexcept
        {
            return &current;
        }

        constexpr EnumIterator & operator++() noexcept
        {
            current = E(underlyingVal(current) + 1u);
            return *this;
        };

        constexpr EnumIterator operator++(int) noexcept
        {
            EnumIterator temp{*this};
            ++*this;
            return temp;
        }

        constexpr bool operator==(const EnumIterator &) const noexcept = default;
    };

    // Ensure `EnumIterator<const E>` resolves to same type as `EnumIterator<E>`
    template <CountableEnum E> requires (std::is_const_v<E>) struct EnumIterator<E> : EnumIterator<std::remove_const_t<E>> {};

    template <CountableEnum E>
    struct EnumIteration
    {
        constexpr EnumIterator<E> begin() const noexcept
        {
            return {};
        }

        constexpr EnumIterator<E> end() const noexcept
        {
            return {E::_n};
        }
    };

    // Ensure `EnumIteration<const E>` resolves to same type as `EnumIteration<E>`
    template <CountableEnum E> requires (std::is_const_v<E>) struct EnumIteration<E> : EnumIteration<std::remove_const_t<E>> {};

    template <CountableEnum E> constexpr EnumIteration<E> iterateEnum{};

    #endif

    namespace _minutia
    {
        template <CountableEnum E>
        constexpr std::array<E, enumCount<E>> makeEnumArray() noexcept
        {
            std::array<E, enumCount<E>> array{};
            for (E e{}; e != E::_n; e = E(underlyingVal(e) + 1u)) {
                array[underlyingVal(e)] = e;
            }
            return array;
        }
    }

    template <CountableEnum E> constexpr std::array<E, enumCount<E>> enumArray{_minutia::makeEnumArray<E>()};
}
