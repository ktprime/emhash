// Copyright 2021, 2022 Peter Dimov.
// Copyright 2023 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt


// http://mattmahoney.net/dc/textdata.html
// http://mattmahoney.net/dc/enwik9.zip

#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING

//#include <boost/unordered_map.hpp>
//#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include "martin/robin_hood.h"
#include "martin/unordered_dense.h"
#include "hash_table8.hpp"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"

#include "util.h"
#include "emilib/emilib2s.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2ss.hpp"

#include <iomanip>
#include <chrono>

using namespace std::chrono_literals;

static std::vector<std::pair<uint32_t, uint32_t>> words;//start, offset
static char* gbuffer = nullptr;//all words buffer

static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, std::size_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();

    std::cout << "\t" << label << ": " << ( t2 - t1 ) / 1ms << " ms (s=" << s << ", size=" << size << ")";

    t1 = t2;
}

static void init_words(const char* fn)
{
    std::cout << fn << " download from http://mattmahoney.net/dc/textdata.html\n";
    auto t1 = std::chrono::steady_clock::now();

    std::ifstream is( fn );
    std::string line;

    uint64_t sums = 0; gbuffer = (char*)malloc("./enwik9" == fn ? (700 << 20) : (70 << 20));

    while (std::getline(is, line))
    {
        int start = -1;
        auto generate_words = [&](int cindex) {
            const auto size = uint32_t(cindex - start);
            memcpy(gbuffer + sums, line.data() + start, size);
            words.emplace_back(sums, size);
            sums += size;
            start = -1;
        };
        for (int i = 0; i < (int)line.size(); i++) {
            const auto c = line[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '.' || c == '-')) {
                if (start < 0) start = i;
            }
            else if (start >= 0) {
                generate_words(i);
            }
        }
        if (start >= 0)
            generate_words(line.size());
    }

    is.close();
    auto t2 = std::chrono::steady_clock::now();
    std::cout << fn << ": " << words.size() << " words, memory = "
        << sums / (1 << 20) << " MB," << ( t2 - t1 ) / 1ms << " ms\n\n";
}

#if 0
#include <boost/regex.hpp>
static void init_words2(int argc)
{
    char const* fn = argc == 8 ? "./enwik8" : "./enwik9";

    auto t1 = std::chrono::steady_clock::now();

    std::ifstream is( fn );
    std::string in( std::istreambuf_iterator<char>( is ), std::istreambuf_iterator<char>{} );
    is.close();
    std::cout << "in.size " << in.size() << ", pls wait ~60 sec ... ";

    boost::regex re( "[a-zA-Z]+");
    boost::sregex_token_iterator it( in.begin(), in.end(), re, 0 ), end;
    in.clear();

    std::vector<std::string> words2;
    words2.reserve(141176632);
    words2.assign( it, end );
    std::cout << ", init_words download from http://mattmahoney.net/dc/textdata.html\n";

    uint64_t sums = 0; for (auto& s:words2) sums += s.size();
    gbuffer = (char*)malloc(sums + 1);
    words.reserve(words2.size());

    sums = 0;
    for (auto& s:words2) {
        memcpy(gbuffer + sums, s.data(), s.size());
        words.emplace_back(sums, s.size());
        sums += s.size();
        s.clear();
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cout << fn << ": " << words.size() << " words, memory = "
        << sums / (1 << 20) << " MB," << ( t2 - t1 ) / 1ms << " ms\n\n";
}
#endif

template<class Map> void test_word_count( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    for( auto const& word: words )
    {
        std::string_view w(gbuffer + word.first, word.second);
        ++map[ w ];
    }

    print_time( t1, "Word count", words.size(), map.size() );
}

template<class Map> void test_contains( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::size_t s = 0;

    for( auto const& word: words )
    {
        std::string_view w(gbuffer + word.first, word.second);
//        w2.remove_prefix( 1 );
        s += map.contains( w );
    }

    print_time( t1, "Contains", s, map.size() );
}

template<class Map> void test_count( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::size_t s = 0;

    for( auto const& word: words )
    {
        std::string_view w(gbuffer + word.first + 1, word.second - 1);
        s += map.count( w );
    }

    print_time( t1, "Count", s, map.size() );
}

template<class Map> void test_iteration( Map& map, std::chrono::steady_clock::time_point & t1 )
{
    std::size_t max = 0;
    std::string_view word;

    for( auto const& x: map )
    {
        if( x.second > max )
        {
            word = x.first;
            max = x.second;
        }
    }

//    print_time( t1, "Iterate and find max element", max, map.size() );
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

template<template<class...> class Map> void test( char const* label )
{
    std::cout << label << ":\n";

    s_alloc_bytes = 0;
    s_alloc_count = 0;

    Map<std::string_view, uint32_t> map;
    map.reserve(words.size() / 100);

    auto t0 = std::chrono::steady_clock::now();
    auto t1 = t0;

    test_word_count( map, t1 );

    if (s_alloc_bytes > 0)
    std::cout << "Memory: " << s_alloc_bytes << " bytes in " << s_alloc_count << " allocations\n\n";

    record rec = { label, 0, s_alloc_bytes, s_alloc_count };

    test_contains( map, t1 );
    test_count( map, t1 );
    test_iteration( map, t1 );

    auto tN = std::chrono::steady_clock::now();
    std::cout << "\tTotal: " << ( tN - t0 ) / 1ms << " ms|load_factor = " << map.load_factor() << " \n\n";

    rec.time_ = ( tN - t0 ) / 1ms;
    times.push_back( rec );
}

// aliases using the counting allocator
#if ABSL_HASH
    #define BstrHasher absl::Hash<K>
#elif BOOST_HASH
    #define BstrHasher boost::hash<K>
#elif HOOD_HASH
    #define BstrHasher robin_hood::hash<K>
#elif STD_HASH
    #define BstrHasher std::hash<K>
#else
    #define BstrHasher ankerl::unordered_dense::hash<K>
#endif

template<class K, class V> using allocator_for = ::allocator< std::pair<K const, V> >;

template<class K, class V> using std_unordered_map =
    std::unordered_map<K, V, BstrHasher>;

//template<class K, class V> using boost_unordered_map =
//    boost::unordered_map<K, V, BstrHasher, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_flat_map =
    boost::unordered_flat_map<K, V, BstrHasher>;

template<class K, class V> using emhash_map8 = emhash8::HashMap<K, V, BstrHasher>;
template<class K, class V> using emhash_map7 = emhash7::HashMap<K, V, BstrHasher>;
template<class K, class V> using emhash_map6 = emhash6::HashMap<K, V, BstrHasher>;
template<class K, class V> using emhash_map5 = emhash5::HashMap<K, V, BstrHasher>;

template<class K, class V> using martin_flat = robin_hood::unordered_map<K, V, BstrHasher>;
template<class K, class V> using martin_dense = ankerl::unordered_dense::map<K, V, BstrHasher>;
template<class K, class V> using emilib1_map = emilib::HashMap<K, V, BstrHasher>;
template<class K, class V> using emilib2_map = emilib2::HashMap<K, V, BstrHasher>;
template<class K, class V> using emilib3_map = emilib3::HashMap<K, V, BstrHasher>;

// fnv1a_hash

template<int Bits> struct fnv1a_hash_impl;

template<> struct fnv1a_hash_impl<32>
{
    std::size_t operator()( std::string_view const& s ) const
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
    std::size_t operator()( std::string_view const& s ) const
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

//template<class K, class V> using std_unordered_map_fnv1a =
//std::unordered_map<K, V, fnv1a_hash, allocator_for<K, V>>;

//template<class K, class V> using boost_unordered_map_fnv1a =
//    boost::unordered_map<K, V, fnv1a_hash, allocator_for<K, V>>;

template<class K, class V> using boost_unordered_flat_map_fnv1a =
    boost::unordered_flat_map<K, V, fnv1a_hash>;

#ifdef ABSL_HMAP
template<class K, class V> using absl_flat_hash_map = absl::flat_hash_map<K, V, BstrHasher>;
template<class K, class V> using absl_flat_hash_map_fnv1a = absl::flat_hash_map<K, V, fnv1a_hash>;
#endif

int main(int argc, const char* argv[])
{
    printInfo(nullptr);

    init_words(argc > 1 ? argv[1] : "./enwik9");

//    test<std_unordered_map>( "std::unordered_map" );
//    test<boost_unordered_map>( "boost::unordered_map" );

    test<emilib3_map> ("emilib3_map" );
    test<emilib2_map> ("emilib2_map" );
    test<emilib1_map> ("emilib1_map" );
    test<boost_unordered_flat_map>( "boost::unordered_flat_map" );
    test<emhash_map7>( "emhash7::hash_map" );
    test<emhash_map5>( "emhash5::hash_map" );
    test<martin_dense>("martin::dense_hash_map" );
    test<martin_flat>("martin::flat_hash_map" );


#ifdef ABSL_HMAP
    test<absl_flat_hash_map>( "absl::flat_hash_map" );
    test<absl_flat_hash_map_fnv1a>( "absl::flat_hash_map, FNV-1a" );
#endif

//    test<std_unordered_map_fnv1a>( "std::unordered_map, FNV-1a" );
//    test<boost_unordered_map_fnv1a>( "boost::unordered_map, FNV-1a" );
    test<boost_unordered_flat_map_fnv1a>( "boost::unordered_flat_map, FNV-1a" );
    test<emhash_map6>( "emhash6::hash_map" );
    test<emhash_map8>( "emhash8::hash_map" );

    std::cout << "---\n\n";
    for( auto const& x: times )
    {
        std::cout << std::setw( 35 ) << ( x.label_ + ": " ) << std::setw( 5 ) << x.time_ << " ms\n" ;//<< std::setw( 9 ) << x.bytes_ << " bytes in " << x.count_ << " allocations\n";
    }

    words.clear();
    free(gbuffer);
    return 0;
}

