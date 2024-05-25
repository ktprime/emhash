// Copyright (C) 2023 Christian Mazakas
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_UNORDERED_DETAIL_FOA_NODE_SET_TYPES_HPP
#define BOOST_UNORDERED_DETAIL_FOA_NODE_SET_TYPES_HPP

#include <boost/unordered/detail/foa/element_type.hpp>

#include <boost/core/no_exceptions_support.hpp>

namespace boost {
  namespace unordered {
    namespace detail {
      namespace foa {

        template <class Key, class VoidPtr> struct node_set_types
        {
          using key_type = Key;
          using init_type = Key;
          using value_type = Key;

          static Key const& extract(value_type const& key) { return key; }

          using element_type = foa::element_type<value_type, VoidPtr>;

          static value_type& value_from(element_type const& x) { return *x.p; }
          static Key const& extract(element_type const& k) { return *k.p; }
          static element_type&& move(element_type& x) { return std::move(x); }
          static value_type&& move(value_type& x) { return std::move(x); }

          template <class A>
          static void construct(
            A& al, element_type* p, element_type const& copy)
          {
            construct(al, p, *copy.p);
          }

          template <typename Allocator>
          static void construct(
            Allocator&, element_type* p, element_type&& x) noexcept
          {
            p->p = x.p;
            x.p = nullptr;
          }

          template <class A, class... Args>
          static void construct(A& al, value_type* p, Args&&... args)
          {
            std::allocator_traits<std::remove_cvref_t<decltype(al)>>::construct(al, p, std::forward<Args>(args)...);
          }

          template <class A, class... Args>
          static void construct(A& al, element_type* p, Args&&... args)
          {
            p->p = std::allocator_traits<std::remove_cvref_t<decltype(al)>>::allocate(al, 1);
            BOOST_TRY
            {
              std::allocator_traits<std::remove_cvref_t<decltype(
                al)>>::construct(
                al, std::to_address(p->p), std::forward<Args>(args)...);
            }
            BOOST_CATCH(...)
            {
              std::allocator_traits<std::remove_cvref_t<decltype(al)>>::deallocate(al, p->p, 1);
              BOOST_RETHROW
            }
            BOOST_CATCH_END
          }

          template <class A> static void destroy(A& al, value_type* p) noexcept
          {
            std::allocator_traits<std::remove_cvref_t<decltype(al)>>::destroy(al, p);
          }

          template <class A>
          static void destroy(A& al, element_type* p) noexcept
          {
            if (p->p) {
              destroy(al, std::to_address(p->p));
              std::allocator_traits<std::remove_cvref_t<decltype(al)>>::deallocate(al, p->p, 1);
            }
          }
        };

      } // namespace foa
    }   // namespace detail
  }     // namespace unordered
} // namespace boost

#endif // BOOST_UNORDERED_DETAIL_FOA_NODE_SET_TYPES_HPP
