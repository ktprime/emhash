// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_HASH_IS_CONTIGUOUS_RANGE_HPP_INCLUDED
#define BOOST_HASH_IS_CONTIGUOUS_RANGE_HPP_INCLUDED

#include <boost/container_hash/is_range.hpp>
#include <boost/minconfig.hpp>
#include <type_traits>


#include <iterator>

namespace boost
{
namespace hash_detail
{

template<class It, class T, class S>
    std::integral_constant< bool, std::is_same<typename std::iterator_traits<It>::value_type, T>::value && std::is_integral<S>::value >
        is_contiguous_range_check( It first, It last, T const*, T const*, S );

template<class T> decltype( is_contiguous_range_check( std::declval<T const&>().begin(), std::declval<T const&>().end(), std::declval<T const&>().data(), std::declval<T const&>().data() + std::declval<T const&>().size(), std::declval<T const&>().size() ) ) is_contiguous_range_( int );
template<class T> std::false_type is_contiguous_range_( ... );

template<class T> struct is_contiguous_range: decltype( hash_detail::is_contiguous_range_<T>( 0 ) )
{
};

} // namespace hash_detail

namespace container_hash
{

template<class T> struct is_contiguous_range: std::integral_constant< bool, is_range<T>::value && hash_detail::is_contiguous_range<T>::value >
{
};

} // namespace container_hash
} // namespace boost


#endif // #ifndef BOOST_HASH_IS_CONTIGUOUS_RANGE_HPP_INCLUDED
