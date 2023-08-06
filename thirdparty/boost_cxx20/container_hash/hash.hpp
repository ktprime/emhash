// Copyright 2005-2014 Daniel James.
// Copyright 2021, 2022 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Based on Peter Dimov's proposal
// http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2005/n1756.pdf
// issue 6.18.

#ifndef BOOST_FUNCTIONAL_HASH_HASH_HPP
#define BOOST_FUNCTIONAL_HASH_HASH_HPP

#include <boost/container_hash/hash_fwd.hpp>
#include <boost/container_hash/is_range.hpp>
#include <boost/container_hash/is_contiguous_range.hpp>
#include <boost/container_hash/is_unordered_range.hpp>
#include <boost/container_hash/detail/hash_tuple_like.hpp>
#include <boost/container_hash/detail/hash_mix.hpp>
#include <boost/container_hash/detail/hash_range.hpp>

#include <string>
#include <iterator>
#include <complex>
#include <utility>
#include <limits>
#include <climits>
#include <cstring>

# include <memory>

#include <typeindex>

#include <system_error>

#include <optional>

#include <variant>

# include <string_view>

namespace boost
{

    //
    // boost::hash_value
    //

    // integral types

    namespace hash_detail
    {
        template<class T,
            bool bigger_than_size_t = (sizeof(T) > sizeof(std::size_t)),
            bool is_unsigned = std::is_unsigned<T>::value,
            std::size_t size_t_bits = sizeof(std::size_t) * CHAR_BIT,
            std::size_t type_bits = sizeof(T) * CHAR_BIT>
        struct hash_integral_impl;

        template<class T, bool is_unsigned, std::size_t size_t_bits, std::size_t type_bits> struct hash_integral_impl<T, false, is_unsigned, size_t_bits, type_bits>
        {
            static std::size_t fn( T v )
            {
                return static_cast<std::size_t>( v );
            }
        };

        template<class T, std::size_t size_t_bits, std::size_t type_bits> struct hash_integral_impl<T, true, false, size_t_bits, type_bits>
        {
            static std::size_t fn( T v )
            {
                typedef typename std::make_unsigned<T>::type U;

                if( v >= 0 )
                {
                    return hash_integral_impl<U>::fn( static_cast<U>( v ) );
                }
                else
                {
                    return ~hash_integral_impl<U>::fn( static_cast<U>( ~static_cast<U>( v ) ) );
                }
            }
        };

        template<class T> struct hash_integral_impl<T, true, true, 32, 64>
        {
            static std::size_t fn( T v )
            {
                std::size_t seed = 0;

                seed = static_cast<std::size_t>( v >> 32 ) + hash_detail::hash_mix( seed );
                seed = static_cast<std::size_t>( v  & 0xFFFFFFFF ) + hash_detail::hash_mix( seed );

                return seed;
            }
        };

        template<class T> struct hash_integral_impl<T, true, true, 32, 128>
        {
            static std::size_t fn( T v )
            {
                std::size_t seed = 0;

                seed = static_cast<std::size_t>( v >> 96 ) + hash_detail::hash_mix( seed );
                seed = static_cast<std::size_t>( v >> 64 ) + hash_detail::hash_mix( seed );
                seed = static_cast<std::size_t>( v >> 32 ) + hash_detail::hash_mix( seed );
                seed = static_cast<std::size_t>( v ) + hash_detail::hash_mix( seed );

                return seed;
            }
        };

        template<class T> struct hash_integral_impl<T, true, true, 64, 128>
        {
            static std::size_t fn( T v )
            {
                std::size_t seed = 0;

                seed = static_cast<std::size_t>( v >> 64 ) + hash_detail::hash_mix( seed );
                seed = static_cast<std::size_t>( v ) + hash_detail::hash_mix( seed );

                return seed;
            }
        };

    } // namespace hash_detail

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value, std::size_t>::type
        hash_value( T v )
    {
        return hash_detail::hash_integral_impl<T>::fn( v );
    }

    // enumeration types

    template <typename T>
    typename std::enable_if<std::is_enum<T>::value, std::size_t>::type
        hash_value( T v )
    {
        // This should in principle return the equivalent of
        //
        // boost::hash_value( to_underlying(v) );
        //
        // However, the C++03 implementation of underlying_type,
        //
        // conditional<is_signed<T>, make_signed<T>, make_unsigned<T>>::type::type
        //
        // generates a legitimate -Wconversion warning in is_signed,
        // because -1 is not a valid enum value when all the enumerators
        // are nonnegative.
        //
        // So the legacy implementation will have to do for now.

        return static_cast<std::size_t>( v );
    }

    // floating point types

    namespace hash_detail
    {
        template<class T,
            std::size_t Bits = sizeof(T) * CHAR_BIT,
            int Digits = std::numeric_limits<T>::digits>
        struct hash_float_impl;

        // float
        template<class T, int Digits> struct hash_float_impl<T, 32, Digits>
        {
            static std::size_t fn( T v )
            {
                std::uint32_t w;
                std::memcpy( &w, &v, sizeof( v ) );

                return w;
            }
        };

        // double
        template<class T, int Digits> struct hash_float_impl<T, 64, Digits>
        {
            static std::size_t fn( T v )
            {
                std::uint64_t w;
                std::memcpy( &w, &v, sizeof( v ) );

                return hash_value( w );
            }
        };

        // 80 bit long double in 12 bytes
        template<class T> struct hash_float_impl<T, 96, 64>
        {
            static std::size_t fn( T v )
            {
                std::uint64_t w[ 2 ] = {};
                std::memcpy( &w, &v, 80 / CHAR_BIT );

                std::size_t seed = 0;

                seed = hash_value( w[0] ) + hash_detail::hash_mix( seed );
                seed = hash_value( w[1] ) + hash_detail::hash_mix( seed );

                return seed;
            }
        };

        // 80 bit long double in 16 bytes
        template<class T> struct hash_float_impl<T, 128, 64>
        {
            static std::size_t fn( T v )
            {
                std::uint64_t w[ 2 ] = {};
                std::memcpy( &w, &v, 80 / CHAR_BIT );

                std::size_t seed = 0;

                seed = hash_value( w[0] ) + hash_detail::hash_mix( seed );
                seed = hash_value( w[1] ) + hash_detail::hash_mix( seed );

                return seed;
            }
        };

        // 128 bit long double
        template<class T, int Digits> struct hash_float_impl<T, 128, Digits>
        {
            static std::size_t fn( T v )
            {
                std::uint64_t w[ 2 ];
                std::memcpy( &w, &v, sizeof( v ) );

                std::size_t seed = 0;

#if defined(__FLOAT_WORD_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__

                seed = hash_value( w[1] ) + hash_detail::hash_mix( seed );
                seed = hash_value( w[0] ) + hash_detail::hash_mix( seed );

#else

                seed = hash_value( w[0] ) + hash_detail::hash_mix( seed );
                seed = hash_value( w[1] ) + hash_detail::hash_mix( seed );

#endif
                return seed;
            }
        };

    } // namespace hash_detail

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, std::size_t>::type
        hash_value( T v )
    {
        return boost::hash_detail::hash_float_impl<T>::fn( v + 0 );
    }

    // pointer types

    // `x + (x >> 3)` adjustment by Alberto Barbati and Dave Harris.
    template <class T> std::size_t hash_value( T* const& v )
    {
        std::uintptr_t x = reinterpret_cast<std::uintptr_t>( v );
        return boost::hash_value( x + (x >> 3) );
    }

    // array types

    template<class T, std::size_t N>
    inline std::size_t hash_value( T const (&x)[ N ] )
    {
        return boost::hash_range( x, x + N );
    }

    template<class T, std::size_t N>
    inline std::size_t hash_value( T (&x)[ N ] )
    {
        return boost::hash_range( x, x + N );
    }

    // complex

    template <class T>
    std::size_t hash_value( std::complex<T> const& v )
    {
        std::size_t re = boost::hash<T>()( v.real() );
        std::size_t im = boost::hash<T>()( v.imag() );

        return re + hash_detail::hash_mix( im );
    }

    // pair

    template <class A, class B>
    std::size_t hash_value( std::pair<A, B> const& v )
    {
        std::size_t seed = 0;

        boost::hash_combine( seed, v.first );
        boost::hash_combine( seed, v.second );

        return seed;
    }

    // ranges (list, set, deque...)

    template <typename T>
    typename std::enable_if<container_hash::is_range<T>::value && !container_hash::is_contiguous_range<T>::value && !container_hash::is_unordered_range<T>::value, std::size_t>::type
        hash_value( T const& v )
    {
        return boost::hash_range( v.begin(), v.end() );
    }

    // contiguous ranges (string, vector, array)

    template <typename T>
    typename std::enable_if<container_hash::is_contiguous_range<T>::value, std::size_t>::type
        hash_value( T const& v )
    {
        return boost::hash_range( v.data(), v.data() + v.size() );
    }

    // unordered ranges (unordered_set, unordered_map)

    template <typename T>
    typename std::enable_if<container_hash::is_unordered_range<T>::value, std::size_t>::type
        hash_value( T const& v )
    {
        return boost::hash_unordered_range( v.begin(), v.end() );
    }

#if (  ( defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION < 142 ) ||  ( !defined(_MSVC_STL_VERSION) && defined(_CPPLIB_VER) && _CPPLIB_VER >= 520 ) )

    // resolve ambiguity with unconstrained stdext::hash_value in <xhash> :-/

    template<template<class...> class L, class... T>
    typename std::enable_if<container_hash::is_range<L<T...>>::value && !container_hash::is_contiguous_range<L<T...>>::value && !container_hash::is_unordered_range<L<T...>>::value, std::size_t>::type
        hash_value( L<T...> const& v )
    {
        return boost::hash_range( v.begin(), v.end() );
    }

    // contiguous ranges (string, vector, array)

    template<template<class...> class L, class... T>
    typename std::enable_if<container_hash::is_contiguous_range<L<T...>>::value, std::size_t>::type
        hash_value( L<T...> const& v )
    {
        return boost::hash_range( v.data(), v.data() + v.size() );
    }

    template<template<class, std::size_t> class L, class T, std::size_t N>
    typename std::enable_if<container_hash::is_contiguous_range<L<T, N>>::value, std::size_t>::type
        hash_value( L<T, N> const& v )
    {
        return boost::hash_range( v.data(), v.data() + v.size() );
    }

    // unordered ranges (unordered_set, unordered_map)

    template<template<class...> class L, class... T>
    typename std::enable_if<container_hash::is_unordered_range<L<T...>>::value, std::size_t>::type
        hash_value( L<T...> const& v )
    {
        return boost::hash_unordered_range( v.begin(), v.end() );
    }

#endif

    // std::unique_ptr, std::shared_ptr


    template <typename T>
    std::size_t hash_value( std::shared_ptr<T> const& x )
    {
        return boost::hash_value( x.get() );
    }

    template <typename T, typename Deleter>
    std::size_t hash_value( std::unique_ptr<T, Deleter> const& x )
    {
        return boost::hash_value( x.get() );
    }


    // std::type_index


    inline std::size_t hash_value( std::type_index const& v )
    {
        return v.hash_code();
    }


    // std::error_code, std::error_condition


    inline std::size_t hash_value( std::error_code const& v )
    {
        std::size_t seed = 0;

        boost::hash_combine( seed, v.value() );
        boost::hash_combine( seed, &v.category() );

        return seed;
    }

    inline std::size_t hash_value( std::error_condition const& v )
    {
        std::size_t seed = 0;

        boost::hash_combine( seed, v.value() );
        boost::hash_combine( seed, &v.category() );

        return seed;
    }


    // std::nullptr_t


    template <typename T>
    typename std::enable_if<std::is_same<T, std::nullptr_t>::value, std::size_t>::type
        hash_value( T const& /*v*/ )
    {
        return boost::hash_value( static_cast<void*>( nullptr ) );
    }


    // std::optional


    template <typename T>
    std::size_t hash_value( std::optional<T> const& v )
    {
        if( !v )
        {
            // Arbitray value for empty optional.
            return 0x12345678;
        }
        else
        {
            return boost::hash<T>()(*v);
        }
    }


    // std::variant


    inline std::size_t hash_value( std::monostate )
    {
        return 0x87654321;
    }

    template <typename... Types>
    std::size_t hash_value( std::variant<Types...> const& v )
    {
        std::size_t seed = 0;

        hash_combine( seed, v.index() );
        std::visit( [&seed](auto&& x) { hash_combine(seed, x); }, v );

        return seed;
    }


    //
    // boost::hash_combine
    //

    template <class T>
    inline void hash_combine( std::size_t& seed, T const& v )
    {
        seed = boost::hash_detail::hash_mix( seed + 0x9e3779b9 + boost::hash<T>()( v ) );
    }

    //
    // boost::hash_range
    //

    template <class It>
    inline void hash_range( std::size_t& seed, It first, It last )
    {
        seed = hash_detail::hash_range( seed, first, last );
    }

    template <class It>
    inline std::size_t hash_range( It first, It last )
    {
        std::size_t seed = 0;

        hash_range( seed, first, last );

        return seed;
    }

    //
    // boost::hash_unordered_range
    //

    template <class It>
    inline void hash_unordered_range( std::size_t& seed, It first, It last )
    {
        std::size_t r = 0;
        std::size_t const s2( seed );

        for( ; first != last; ++first )
        {
            std::size_t s3( s2 );

            hash_combine<typename std::iterator_traits<It>::value_type>( s3, *first );

            r += s3;
        }

        seed += r;
    }

    template <class It>
    inline std::size_t hash_unordered_range( It first, It last )
    {
        std::size_t seed = 0;

        hash_unordered_range( seed, first, last );

        return seed;
    }

    //
    // boost::hash
    //

    template <class T> struct hash
    {
        typedef T argument_type;
        typedef std::size_t result_type;

        std::size_t operator()( T const& val ) const
        {
            return hash_value( val );
        }
    };

#if (  ( defined(_MSVC_STL_VERSION) && _MSVC_STL_VERSION < 142 ) ||  ( !defined(_MSVC_STL_VERSION) && defined(_CPPLIB_VER) && _CPPLIB_VER >= 520 ) )

    // Dinkumware has stdext::hash_value for basic_string in <xhash> :-/

    template<class E, class T, class A> struct hash< std::basic_string<E, T, A> >
    {
        typedef std::basic_string<E, T, A> argument_type;
        typedef std::size_t result_type;

        std::size_t operator()( std::basic_string<E, T, A> const& val ) const
        {
            return boost::hash_value( val );
        }
    };

#endif

    // boost::unordered::hash_is_avalanching

    namespace unordered
    {
        template<class T> struct hash_is_avalanching;
        template<class Ch> struct hash_is_avalanching< boost::hash< std::basic_string<Ch> > >: std::is_integral<Ch> {};

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
        template<> struct hash_is_avalanching< boost::hash< std::basic_string<char8_t> > >: std::true_type {};
#endif


        template<class Ch> struct hash_is_avalanching< boost::hash< std::basic_string_view<Ch> > >: std::is_integral<Ch> {};

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
        template<> struct hash_is_avalanching< boost::hash< std::basic_string_view<char8_t> > >: std::true_type {};
#endif

    } // namespace unordered

} // namespace boost

#endif // #ifndef BOOST_FUNCTIONAL_HASH_HASH_HPP
