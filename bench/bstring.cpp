// Copyright 2021 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/config.hpp>
#ifdef ABSL_HMAP
# include "absl/container/node_hash_map.h"
# include "absl/container/flat_hash_map.h"
#endif
#ifdef HAVE_TSL_HOPSCOTCH
# include "tsl/hopscotch_map.h"
#endif
#ifdef HAVE_TSL_ROBIN
# include "tsl/robin_map.h"
#endif

#include "./util.h"
#include "martin/robin_hood.h"
#include "martin/unordered_dense.h"

#include "../hash_table8.hpp"
#include "../hash_table7.hpp"
#include "../hash_table5.hpp"

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

#include <unordered_map>
#include <vector>
#include <memory>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace std::chrono_literals;

static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, std::uint32_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();

    std::cout << "\t" << label << ": " << ( t2 - t1 ) / 1ms << " ms";// (size=" << size << ")";
    if (s == 123) std::cout << " err:";

    t1 = t2;
}

static unsigned N = 2'000'000;
static int K = 10;

static std::vector<std::string> indices1, indices2;

static std::string make_index( unsigned x )
{
    char buffer[ 64 ];
    std::snprintf( buffer, sizeof(buffer), "pfx_%u_sfx", x );

    return buffer;
}

static std::string make_random_index( unsigned x )
{
    char buffer[ 64 ];
    std::snprintf( buffer, sizeof(buffer), "pfx_%0*d_%u_sfx", x % 8 + 1, 0, x );

    return buffer;
}

static void init_indices()
{
    indices1.reserve( N*2+1 );
    indices1.push_back( make_index( 0 ) );

    for( unsigned i = 1; i <= N*2; ++i )
    {
        indices1.push_back( make_index( i ) );
    }

    indices2.reserve( N*2+1 );
    indices2.push_back( make_index( 0 ) );

    {
        boost::detail::splitmix64 rng;

        for( unsigned i = 1; i <= N*2; ++i )
        {
            indices2.push_back( make_random_index( static_cast<std::uint32_t>( rng() ) ) );
        }
    }
}

template<class Map> BOOST_NOINLINE void test_insert( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    for( unsigned i = 1; i <= N; ++i )
    {
        map.insert( { indices1[ i ], i } );
    }

    print_time( t1, "Consecutive insert",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.emplace(  indices2[ i ], i  );
    }

    print_time( t1, "Random insert",  0, map.size() );

//    std::cout << std::endl;
}

template<class Map> BOOST_NOINLINE void test_lookup( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::uint32_t s;

    s = 0;

    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            s += map.count( indices1[ i ] );
        }
    }

    print_time( t1, "Consecutive lookup",  s, map.size() );

    s = 0;

    for( int j = 0; j < K; ++j )
    {
        for( unsigned i = 1; i <= N * 2; ++i )
        {
            auto it = map.find( indices2[ i ] );
            if( it != map.end() ) s += it->second;
        }
    }

    print_time( t1, "Random lookup",  s, map.size() );

//    std::cout << std::endl;
}

template<class Map> BOOST_NOINLINE void test_iteration( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    auto it = map.begin();

    while( it != map.end() )
    {
        if( it->second & 1 )
        {
            if constexpr( std::is_void_v< decltype( map.erase( it ) ) > )
            {
                map.erase( it++ );
            }
            else
            {
                it = map.erase( it );
            }
        }
        else
        {
            ++it;
        }
    }

    print_time( t1, "Iterate and erase odd elements",  0, map.size() );

    std::cout << std::endl;
}

template<class Map> BOOST_NOINLINE void test_erase( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices1[ i ] );
    }

    print_time( t1, "Consecutive erase",  0, map.size() );

    for( unsigned i = 1; i <= N; ++i )
    {
        map.erase( indices2[ i ] );
    }

    print_time( t1, "Random erase",  0, map.size() );

    std::cout << std::endl;
}

// counting allocator

static std::size_t s_alloc_bytes = 0;
static std::size_t s_alloc_count = 0;

template<class T> struct allocator
{
    using value_type = T;

    allocator() = default;

    template<class U> allocator( allocator<U> const & ) noexcept
    {
    }

    template<class U> bool operator==( allocator<U> const & ) const noexcept
    {
        return true;
    }

    template<class U> bool operator!=( allocator<U> const& ) const noexcept
    {
        return false;
    }

    T* allocate( std::size_t n ) const
    {
        s_alloc_bytes += n * sizeof(T);
        s_alloc_count++;

        return std::allocator<T>().allocate( n );
    }

    void deallocate( T* p, std::size_t n ) const noexcept
    {
        s_alloc_bytes -= n * sizeof(T);
        s_alloc_count--;

        std::allocator<T>().deallocate( p, n );
    }
};

//

struct record
{
    std::string label_;
    long long time_;
    std::size_t bytes_;
    std::size_t count_;
};

static std::vector<record> times;

#if STD_VIEW || 1
#include <string_view>
using keyType = std::string_view;
#else
using keyType = std::string;
#endif

template<template<class...> class Map> BOOST_NOINLINE void test( char const* label )
{
    s_alloc_bytes = 0;
    s_alloc_count = 0;

    Map<keyType, std::uint32_t> map;

    auto t0 = std::chrono::steady_clock::now();
    auto t1 = t0;

    test_insert( map, t1 );

    if (s_alloc_bytes > 0)
    std::cout << "Memory: " << s_alloc_bytes << " bytes in " << s_alloc_count << " allocations\n";

    record rec = { label, 0, s_alloc_bytes, s_alloc_count };

    test_lookup( map, t1 );
    test_iteration( map, t1 );
    test_lookup( map, t1 );
    test_erase( map, t1 );

    auto tN = std::chrono::steady_clock::now();
    rec.time_ = ( tN - t0 ) / 1ms;
    times.push_back( rec );
    std::cout << (tN - t0) / 1ms << " ms ";
    std::cout << label << ":\n\n";
}

#if BOOST_HASH
    #define BstrHasher boost::hash<K>
#elif HOOD_HASH
    #define BstrHasher robin_hood::hash<K>
#elif STD_HASH
    #define BstrHasher std::hash<K>
#elif ABSL_HASH
    #define BstrHasher absl::Hash<K>
#else
    #define BstrHasher ankerl::unordered_dense::hash<K>
#endif

// aliases using the counting allocator

template<class K, class V> using allocator_for = ::allocator< std::pair<K const, V> >;

template<class K, class V> using std_unordered_map =
    std::unordered_map<K, V, BstrHasher, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_flat_map =
    boost::unordered_flat_map<K, V, BstrHasher, std::equal_to<K>>;

template<class K, class V> using emhash_map8 = emhash8::HashMap<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using emhash_map7 = emhash7::HashMap<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using emhash_map5 = emhash5::HashMap<K, V, BstrHasher, std::equal_to<K>>;

template<class K, class V> using martin_flat = robin_hood::unordered_map<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using martin_dense = ankerl::unordered_dense::map<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using emilib1_map = emilib::HashMap<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using emilib2_map = emilib2::HashMap<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using emilib3_map = emilib::HashMap<K, V, BstrHasher, std::equal_to<K>>;

#ifdef ABSL_HMAP

template<class K, class V> using absl_node_hash_map = absl::node_hash_map<K, V, BstrHasher, std::equal_to<K>>;
template<class K, class V> using absl_flat_hash_map = absl::flat_hash_map<K, V, BstrHasher, std::equal_to<K>>;

#endif

#ifdef HAVE_TSL_HOPSCOTCH

template<class K, class V> using tsl_hopscotch_map =
    tsl::hopscotch_map<K, V, BstrHasher, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

template<class K, class V> using tsl_hopscotch_pg_map =
    tsl::hopscotch_pg_map<K, V, BstrHasher, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

#endif

#ifdef HAVE_TSL_ROBIN

template<class K, class V> using tsl_robin_map =
    tsl::robin_map<K, V, BstrHasher, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

template<class K, class V> using tsl_robin_pg_map =
    tsl::robin_pg_map<K, V, BstrHasher, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

#endif

// fnv1a_hash

template<int Bits> struct fnv1a_hash_impl;

template<> struct fnv1a_hash_impl<32>
{
    std::size_t operator()( keyType const& s ) const
    {
        std::size_t h = 0x811C9DC5u;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x01000193ul;
        }

        return h;
    }
};

template<> struct fnv1a_hash_impl<64>
{
    std::size_t operator()( keyType const& s ) const
    {
        std::size_t h = 0xCBF29CE484222325ull;

        char const * first = s.data();
        char const * last = first + s.size();

        for( ; first != last; ++first )
        {
            h ^= static_cast<unsigned char>( *first );
            h *= 0x00000100000001B3ull;
        }

        return h;
    }
};

struct fnv1a_hash: fnv1a_hash_impl< std::numeric_limits<std::size_t>::digits >
{
    using is_avalanching = void;
};

template<class K, class V> using std_unordered_map_fnv1a =
std::unordered_map<K, V, fnv1a_hash, std::equal_to<K>, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_flat_map_fnv1a =
    boost::unordered_flat_map<K, V, fnv1a_hash, std::equal_to<K>>;

#ifdef ABSL_HMAP

template<class K, class V> using absl_node_hash_map_fnv1a =
    absl::node_hash_map<K, V, fnv1a_hash, absl::container_internal::hash_default_eq<K>, allocator_for<K, V>>;

template<class K, class V> using absl_flat_hash_map_fnv1a =
    absl::flat_hash_map<K, V, fnv1a_hash, absl::container_internal::hash_default_eq<K>>;

#endif

#ifdef HAVE_TSL_HOPSCOTCH

template<class K, class V> using tsl_hopscotch_map_fnv1a =
    tsl::hopscotch_map<K, V, fnv1a_hash, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

template<class K, class V> using tsl_hopscotch_pg_map_fnv1a =
    tsl::hopscotch_pg_map<K, V, fnv1a_hash, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

#endif

#ifdef HAVE_TSL_ROBIN

template<class K, class V> using tsl_robin_map_fnv1a =
    tsl::robin_map<K, V, fnv1a_hash, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

template<class K, class V> using tsl_robin_pg_map_fnv1a =
    tsl::robin_pg_map<K, V, fnv1a_hash, std::equal_to<K>, ::allocator< std::pair<K, V> >>;

#endif

//

int main(int argc, const char* argv[])
{
    if (argc > 1 && isdigit(argv[1][0]))
        N = atoi(argv[1]);
    if (argc > 2 && isdigit(argv[2][0]))
        K = atoi(argv[2]);

    init_indices();

    printf("N = %d, Loops = %d\n", N, K);

    test<emilib1_map> ("emilib1_map" );
    test<emilib3_map> ("emilib3_map" );
    test<boost_unordered_flat_map>( "boost::unordered_flat_map" );
    test<emilib2_map> ("emilib2_map" );

    test<emhash_map5>( "emhash5::hash_map" );
    test<emhash_map7>( "emhash7::hash_map" );
    test<emhash_map8>( "emhash8::hash_map" );
    test<martin_dense>("martin::dense_hash_map" );
    test<martin_flat>("martin::flat_hash_map" );


#ifdef ABSL_HMAP

    test<absl_node_hash_map>( "absl::node_hash_map" );
    test<absl_flat_hash_map>( "absl::flat_hash_map" );

#endif

#ifdef HAVE_TSL_HOPSCOTCH

    test<tsl_hopscotch_map>( "tsl::hopscotch_map" );
    test<tsl_hopscotch_pg_map>( "tsl::hopscotch_pg_map" );

#endif

#ifdef HAVE_TSL_ROBIN

    test<tsl_robin_map>( "tsl::robin_map" );
    test<tsl_robin_pg_map>( "tsl::robin_pg_map" );

#endif

#ifdef ABSL_HMAP

    test<absl_node_hash_map_fnv1a>( "absl::node_hash_map, FNV-1a" );
    test<absl_flat_hash_map_fnv1a>( "absl::flat_hash_map, FNV-1a" );

#endif

#ifdef HAVE_TSL_HOPSCOTCH

    test<tsl_hopscotch_map_fnv1a>( "tsl::hopscotch_map, FNV-1a" );
    test<tsl_hopscotch_pg_map_fnv1a>( "tsl::hopscotch_pg_map, FNV-1a" );

#endif

#ifdef HAVE_TSL_ROBIN

    test<tsl_robin_map_fnv1a>( "tsl::robin_map, FNV-1a" );
    test<tsl_robin_pg_map_fnv1a>( "tsl::robin_pg_map, FNV-1a" );

#endif

    test<std_unordered_map>( "std::unordered_map" );

    std::cout << "---\n\n";

    for( auto const& x: times )
    {
        std::cout << std::setw( 35 ) << ( x.label_ + ": " ) << std::setw( 5 ) << x.time_ << " ms, " << std::setw( 9 ) << x.bytes_ << " bytes in " << x.count_ << " allocations\n";
    }
}

