// Copyright (C) 2022 Christian Mazakas
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_UNORDERED_DETAIL_TYPE_TRAITS_HPP
#define BOOST_UNORDERED_DETAIL_TYPE_TRAITS_HPP

#include <boost/minconfig.hpp>
#pragma once



// BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES

#if !defined(BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES)
#define BOOST_UNORDERED_TEMPLATE_DEDUCTION_GUIDES 1
#endif


namespace boost {
  namespace unordered {
    namespace detail {
      ////////////////////////////////////////////////////////////////////////////
      // Type checkers used for the transparent member functions added by C++20
      // and up

      template <class, class = void>
      struct is_transparent : public std::false_type
      {
      };

      template <class T>
      struct is_transparent<T,
        typename std::void_t<typename T::is_transparent>>
          : public std::true_type
      {
      };

      template <class, class Hash, class KeyEqual> struct are_transparent
      {
        static bool const value =
          is_transparent<Hash>::value && is_transparent<KeyEqual>::value;
      };

      template <class Key, class UnorderedMap> struct transparent_non_iterable
      {
        typedef typename UnorderedMap::hasher hash;
        typedef typename UnorderedMap::key_equal key_equal;
        typedef typename UnorderedMap::iterator iterator;
        typedef typename UnorderedMap::const_iterator const_iterator;

        static bool const value =
          are_transparent<Key, hash, key_equal>::value &&
          !std::is_convertible<Key, iterator>::value &&
          !std::is_convertible<Key, const_iterator>::value;
      };

      // https://eel.is/c++draft/container.requirements#container.alloc.reqmts-34
      // https://eel.is/c++draft/container.requirements#unord.req.general-243

      template <class InputIterator>
      constexpr bool const is_input_iterator_v =
        !std::is_integral<InputIterator>::value;

      template <class A, class = void> struct is_allocator
      {
        constexpr static bool const value = false;
      };

      template <class A>
      struct is_allocator<A,
        std::void_t<typename A::value_type,
          decltype(std::declval<A&>().allocate(std::size_t{}))> >
      {
        constexpr static bool const value = true;
      };

      template <class A>
      constexpr bool const is_allocator_v = is_allocator<A>::value;

      template <class H>
      constexpr bool const is_hash_v =
        !std::is_integral<H>::value && !is_allocator_v<H>;

      template <class P> constexpr bool const is_pred_v = !is_allocator_v<P>;
    } // namespace detail
  }   // namespace unordered
} // namespace boost

#endif // BOOST_UNORDERED_DETAIL_TYPE_TRAITS_HPP
