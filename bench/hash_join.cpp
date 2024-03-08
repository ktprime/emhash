#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#define _SILENCE_CXX20_CISO646_REMOVED_WARNING
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include "util.h"

#include "tsl/robin_map.h"
#include "martin/robin_hood.h"
#include "martin/unordered_dense.h"
#include "phmap/phmap.h"
#include "rigtorp/rigtorp.hpp"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table8.hpp"
#include "emilib/emilib2s.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2ss.hpp"
#include <iomanip>
#include <chrono>


#if CK_HMAP
#include "util.h"
#include "ck/Common/HashTable/HashMap.h"
#endif

using namespace std::chrono_literals;
#if TKey == 1
using KeyType = uint64_t;
#else
using KeyType = uint32_t;
#endif

#if TVal == 1
using ValType = uint64_t;
#else
using ValType = uint32_t;
#endif

static unsigned N = 123'456'78;

// aliases using the counting allocator
#if BOOST_HASH
    #define BintHasher boost::hash<K>
#elif FIB_HASH
    #define BintHasher Int64Hasher<K>
#elif HOOD_HASH
    #define BintHasher robin_hood::hash<K>
#elif ABSL_HASH
    #define BintHasher absl::Hash<K>
#elif STD_HASH
    #define BintHasher std::hash<K>
#else
    #define BintHasher ankerl::unordered_dense::hash<K>
#endif


static void print_time( std::chrono::steady_clock::time_point & t1, char const* label, uint64_t s, std::size_t size )
{
    auto t2 = std::chrono::steady_clock::now();
    if (size + s)
    std::cout << "\t" << label << ": " << ( t2 - t1 ) / 1ms << " ms";// (s=" << s << ", size=" << size << ")";
    t1 = t2;
}

static std::vector< KeyType > indices1, indices2;

static void init_indices(int n1, int n2, const uint32_t ration = 10)
{
    indices1.resize(n1);
    indices2.resize(n2);

    boost::detail::splitmix64 rng;

    for (size_t i = 0; i < indices1.size(); ++i )
    {
        auto rt = rng();
        indices1[i] = rt;
        if (i % ration == 0) {
            indices2[i] = rt;
        } else {
            indices2[i] = rng();
        }
    }

    for (int i = indices1.size(); i < indices2.size(); i++) {
        indices2[i] = rng();
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices2.begin(), indices2.end(), gen);

    printf("v1 size = %zd, memory = %zd MB\n", indices1.size(),  indices1.size() * sizeof(KeyType) / 1024 / 1024);
    printf("v2 size = %zd, memory = %zd MB\n", indices2.size(), indices2.size() * sizeof(KeyType) / 1024 / 1024);
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

template<template<class...> class Map>  void test_loops(char const* label)
{
    auto t0 = std::chrono::steady_clock::now();
    Map<KeyType, ValType> map(indices1.size());
    for (auto v : indices1) {
        map.emplace(v, (ValType)v);
    }

    auto t1 = std::chrono::steady_clock::now();

    size_t ans = 0;
    for (auto v : indices2) {
        ans += map.count(v);
    }

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, join_loops = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
}

template<template<class...> class Map>  void test_block(char const* label)
{
    auto t0 = std::chrono::steady_clock::now();
    Map<KeyType, ValType> map(indices1.size());
    for (const auto v : indices1) {
        map.emplace(v, (ValType)v);
    }
    auto t1 = std::chrono::steady_clock::now();

#if 0
    size_t ans = 0;
    for (auto v:indices2) {
        ans += map.count(v);
    }
#else
    constexpr uint32_t HASH_MAPS_SIZE = 1 << 10;
    constexpr uint32_t VCACHE_SIZE = 32;
    const uint32_t capacity   = map.bucket_count() - 1;
    const uint32_t block_size = map.bucket_count() / HASH_MAPS_SIZE; //use move

    auto msb32 = [](unsigned int n)
    {
        int ans = 1;
        while (n) {
            ans += 1;
            n /= 2;
        }
        return ans;
    };

    const auto high_bit = msb32(block_size);

    size_t ans = 0;
    std::array<std::array<KeyType, VCACHE_SIZE>, HASH_MAPS_SIZE> vblocks = { 0 };

    for (const auto v2:indices2) {
        const auto vhash = ankerl::unordered_dense::hash<KeyType>()(v2);
        const uint32_t bindex = (vhash & capacity) >> high_bit; // save hash
        auto& bv = vblocks[bindex];
        if (bv[0] >= VCACHE_SIZE - 1) {
            for (int i = 1; i < VCACHE_SIZE; i++) {
                ans += map.count(bv[i]);
            }
            bv[0] = 0;
        }
        bv[++bv[0]] = v2;
    }

    for (const auto& bv:vblocks) {
        for (int i = 1; i <= bv[0]; i++)
            ans += map.count(bv[i]);
    }
#endif

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, join_block = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms,  ans);
}

template<class K, class V> using allocator_for = ::allocator< std::pair<K const, V> >;

template<class K, class V> using boost_unordered_flat_map =
    boost::unordered_flat_map<K, V, BintHasher>;

template<class K, class V> using std_unordered_map =
    std::unordered_map<K, V, BintHasher>;

template<class K, class V> using emhash_map5 = emhash5::HashMap<K, V, BintHasher>;
template<class K, class V> using emhash_map6 = emhash6::HashMap<K, V, BintHasher>;
template<class K, class V> using emhash_map7 = emhash7::HashMap<K, V, BintHasher>;
template<class K, class V> using emhash_map8 = emhash8::HashMap<K, V, BintHasher>;

template<class K, class V> using martin_flat = robin_hood::unordered_map<K, V, BintHasher>;
template<class K, class V> using emilib_map1 = emilib::HashMap<K, V, BintHasher>;
template<class K, class V> using emilib_map2 = emilib2::HashMap<K, V, BintHasher>;
template<class K, class V> using emilib_map3 = emilib3::HashMap<K, V, BintHasher>;

#ifdef CXX20
template<class K, class V> using jg_densemap = jg::dense_hash_map<K, V, BintHasher>;
#endif
template<class K, class V> using martin_dense = ankerl::unordered_dense::map<K, V, BintHasher>;
template<class K, class V> using phmap_flat  = phmap::flat_hash_map<K, V, BintHasher>;
template<class K, class V> using tsl_robin_map= tsl::robin_map<K, V, BintHasher>;

#if ABSL_HMAP
template<class K, class V> using absl_flat_hash_map = absl::flat_hash_map<K, V, BintHasher>;
#endif

template<class K, class V> using rig_hashmap = rigtorp::HashMap<K, V, BintHasher>;

#if CK_HMAP
template<class K, class V> using ck_hashmap = ck::HashMap<K, V, BintHasher>;
#endif

int main(int argc, const char* argv[])
{
    int K = 10, R = 10;
    if (argc > 1 && isdigit(argv[1][0])) N = atoi(argv[1]);
    if (argc > 2 && isdigit(argv[2][0])) K = atoi(argv[2]);
    if (argc > 3 && isdigit(argv[3][0])) R = atoi(argv[3]);

    assert(K > 0 && N > 0 && R > 0);
    init_indices(N, N*K, R);

    test_loops<martin_flat> ("martin_flat" );
//    test_block<martin_flat> ("martin_flat" );

    test_loops<emhash_map5> ("emhash_map5" );
    test_block<emhash_map5>("emhash_map5");

    test_loops<emhash_map6>("emhash_map6");
    test_block<emhash_map6>("emhash_map6");

    test_loops<rig_hashmap>( "rigtorp::hashmap" );
    test_block<rig_hashmap>( "rigtorp::hashmap" );

#if CK_HMAP
    test_loops<ck_hashmap>( "ck::hashmap" );
    test_block<ck_hashmap>( "ck::hashmap" );
#endif

    test_loops<boost_unordered_flat_map>( "boost::flat_hashmap" );
    test_block<boost_unordered_flat_map>("boost::flat_hashmap");

    test_loops<emilib_map1> ("emilib_map1" );
    test_block<emilib_map1> ("emilib_map1");

    test_loops<emilib_map3> ("emilib_map3" );
    test_block<emilib_map3> ("emilib_map3" );

    test_loops<emilib_map2> ("emilib_map2" );
    test_block<emilib_map2> ("emilib_map2" );

    test_loops<emhash_map8>("emhash_map8");
    test_block<emhash_map8>("emhash_map8");

    test_loops<emhash_map7>("emhash_map7");
    test_block<emhash_map7>("emhash_map7");

#if ABSL_HMAP
    test_loops<absl_flat_hash_map>("absl::flat_hash_map" );
    test_block<absl_flat_hash_map>("absl::flat_hash_map" );
#endif
    test_loops<phmap_flat> ("phmap_flat" );
    test_block<phmap_flat> ("phmap_flat" );

    test_loops<std_unordered_map> ("std::unordered_map" );
    test_block<std_unordered_map> ("std::unordered_map" );

#ifdef CXX20
    test_loops<jg_densemap> ("jg_densemap" );
    test_block<jg_densemap>("jg_densemap");
#endif

    test_loops<martin_dense>("martin_dense" );
    test_block<martin_dense>("martin_dense");

    test_loops<tsl_robin_map> ("tsl_robin_map" );
    test_block<tsl_robin_map>("tsl_robin_map");

    return 0;
}
