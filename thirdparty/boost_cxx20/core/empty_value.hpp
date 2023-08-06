/*
Copyright 2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#ifndef BOOST_CORE_EMPTY_VALUE_HPP
#define BOOST_CORE_EMPTY_VALUE_HPP

#include <utility>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4510)
#endif

namespace boost {

template<class T>
struct use_empty_value_base {
    enum {
        value = __is_empty(T) && !__is_final(T)
    };
};

struct empty_init_t { };

namespace empty_ {

template<class T, unsigned N = 0,
    bool E = boost::use_empty_value_base<T>::value>
class empty_value {
public:
    typedef T type;

    empty_value() = default;

    constexpr empty_value(boost::empty_init_t)
        : value_() { }

    template<class U, class... Args>
    constexpr empty_value(boost::empty_init_t, U&& value, Args&&... args)
        : value_(std::forward<U>(value), std::forward<Args>(args)...) { }

    constexpr const T& get() const noexcept {
        return value_;
    }

    constexpr T& get() noexcept {
        return value_;
    }

private:
    T value_;
};

template<class T, unsigned N>
class empty_value<T, N, true>
    : T {
public:
    typedef T type;

    empty_value() = default;

    constexpr empty_value(boost::empty_init_t)
        : T() { }

    template<class U, class... Args>
    constexpr empty_value(boost::empty_init_t, U&& value, Args&&... args)
        : T(std::forward<U>(value), std::forward<Args>(args)...) { }

    constexpr const T& get() const noexcept {
        return *this;
    }

    constexpr T& get() noexcept {
        return *this;
    }
};

} /* empty_ */

using empty_::empty_value;

inline constexpr empty_init_t empty_init = empty_init_t();

} /* boost */

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#endif
