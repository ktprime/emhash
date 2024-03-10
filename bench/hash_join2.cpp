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

static unsigned N = 1'123'4560;

// aliases using the counting allocator
#if BOOST_HASH
    #define BintHasher boost::hash<KeyType>
#elif FIB_HASH
    #define BintHasher Int64Hasher<KeyType>
#elif HOOD_HASH
    #define BintHasher robin_hood::hash<KeyType>
#elif ABSL_HASH
    #define BintHasher absl::Hash<KeyType>
#elif STD_HASH
    #define BintHasher std::hash<KeyType>
#else
    #define BintHasher ankerl::unordered_dense::hash<KeyType>
#endif

static std::vector< KeyType > indices1, indices2;

static void init_indices(int n1, int n2, const uint32_t ration = 10)
{
    auto t0 = std::chrono::steady_clock::now();
    indices1.resize(n1);
    indices2.resize(n2);

    //boost::detail::splitmix64 rng;
    WyRand rng;

    for (size_t i = 0; i < n1; ++i )
    {
        auto rt = rng();
        indices1[i] = rt;
        if (i % ration == 0) {
            indices2[i] = rt;
        } else {
            indices2[i] = rng();
        }
    }

    for (int i = n1; i < n2; i++) {
        indices2[i] = rng();
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices2.begin(), indices2.end(), gen);

    auto t1 = std::chrono::steady_clock::now();
    printf("v1 size = %zd, memory = %zd MB\n", indices1.size(), indices1.size() * sizeof(KeyType) >> 20);
    printf("v2 size = %zd, memory = %zd MB time use %ld ms\n", indices2.size(), indices2.size() * sizeof(KeyType) >> 20, (t1 - t0) / 1ms);
}

static float max_lf = 0.99f;
template<template<class...> class Map>  void test_loops(char const* label)
{
    auto t0 = std::chrono::steady_clock::now();
    Map<KeyType, ValType> map(indices1.size() / 2);
    map.max_load_factor(max_lf);
    for (auto v : indices1) {
        map.emplace(v, (ValType)v);
    }

    auto t1 = std::chrono::steady_clock::now();

    size_t ans = 0;
    for (auto v : indices2) {
        ans += map.count(v);
    }

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, lf = %.2f loops = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, map.load_factor(), ans);
}

constexpr uint32_t HASH_MAPS = 1013u;
constexpr uint32_t BLOCK_SIZE = 256;

template<template<class...> class Map>  void test_block( char const* label )
{
    auto t0 = std::chrono::steady_clock::now();

    Map<KeyType, ValType> map[HASH_MAPS];

    for (auto& v: map) {
        v.reserve(indices1.size() / HASH_MAPS);
        v.max_load_factor(max_lf);
    }
    for (const auto v:indices1)
        map[v % HASH_MAPS].emplace(v, (ValType)v);

    auto t1 = std::chrono::steady_clock::now();

    size_t ans = 0;
    std::array<std::array<KeyType, BLOCK_SIZE>, HASH_MAPS> vblocks = {0};

    for (const auto v2:indices2) {
        const uint32_t bindex = v2 % HASH_MAPS;// (vhash & capacity) % HASH_MAPS; // save hash
        auto& bv = vblocks[bindex];
        if (bv[0] >= BLOCK_SIZE - 1) {
            for (int i = 1; i < BLOCK_SIZE; i++)
                ans += map[bindex].count(bv[i]);// map.count(vhash, bv[i])
            bv[0] = 0;
        }
        bv[++bv[0]] = v2;
    }

    int bindex = 0;
    for (const auto& bv:vblocks) {
        const auto& mapi = map[bindex++];
        for (int i = 1; i <= bv[0]; i++)
            ans += mapi.count(bv[i]);
    }

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, join_block = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
}

template<template<class...> class Map>  void test_block2( char const* label )
{
    auto t0 = std::chrono::steady_clock::now();

    Map<KeyType, ValType> map[HASH_MAPS];

    for (auto& v: map)
        v.reserve(indices1.size() / HASH_MAPS);
    for (const auto v:indices1)
        map[v % HASH_MAPS].emplace(v, (ValType)v);

    auto t1 = std::chrono::steady_clock::now();

    using KeyHashCache = std::pair<uint32_t, KeyType>;

    size_t ans = 0;
    std::array<std::array<KeyHashCache, BLOCK_SIZE>, HASH_MAPS> vblocks = {};

    for (const auto v2:indices2) {
        const uint32_t bindex = v2 % HASH_MAPS;// (vhash & capacity) % HASH_MAPS; // save hash
        auto& bv = vblocks[bindex];
        if (bv[0].first >= BLOCK_SIZE - 1) {
            //std::sort(bv.begin() + 1, bv.begin() + BLOCK_SIZE);
            for (int i = 1; i < BLOCK_SIZE; i++)
                ans += map[bindex].count_hint(bv[i].second, bv[i].first);// map.count(vhash, bv[i])
            bv[0].first = 0;
        }
        const auto hash_bucket = BintHasher()(v2) & (map[bindex].bucket_count() - 1);
        bv[++bv[0].first] = {hash_bucket, v2};
    }

    int bindex = 0;
    for (const auto& bv:vblocks) {
        const auto& mapi = map[bindex++];
        for (int i = 1; i <= bv[0].first; i++)
            ans += mapi.count_hint(bv[i].second, bv[i].first);
    }

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, join_block2 = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
}

template<template<class...> class Map>  void test_block3( char const* label )
{
    auto t0 = std::chrono::steady_clock::now();

    Map<KeyType, ValType> map[HASH_MAPS];

    for (auto& v: map)
        v.reserve(indices1.size() / HASH_MAPS);
    for (const auto v:indices1)
        map[v % HASH_MAPS].emplace(v, (ValType)v);

    auto t1 = std::chrono::steady_clock::now();
    size_t ans = 0;

    for (const auto v2:indices2) {
        ans += map[v2 % HASH_MAPS].count(v2);
    }

    auto tN = std::chrono::steady_clock::now();
    printf("%20s insert %4zd ms, find %4zd ms, join_block2 = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
}

template<class K, class V> using boost_unordered_flat_map = boost::unordered_flat_map<K, V, BintHasher>;

template<class K, class V> using std_unordered_map = std::unordered_map<K, V, BintHasher>;

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

    printInfo(nullptr);
    puts("v1_size(1-10000)M v1 * r(1-10000) hit_rate(1 - 100)\n ex: ./join_hash 60M 10 1\n");

    if (argc > 1 && isdigit(argv[1][0])) N = atoi(argv[1]);
    if (N < 10000) N = (N * 1024 * 1024) / sizeof(KeyType);
    if (argc > 2 && isdigit(argv[2][0])) K = atoi(argv[2]);
    if (argc > 3 && isdigit(argv[3][0])) R = atoi(argv[3]);

    assert(K > 0 && N > 0 && R > 0);
    init_indices(N, N*K, R);

//    test_loops<martin_flat> ("martin_flat" );
//    test_block<martin_flat> ("martin_flat" );

    test_loops<emhash_map5> ("emhash_map5" );
    test_block<emhash_map5>("emhash_map5");
    //test_block2<emhash_map5> ("emhash_map5" );
#if 1
    test_loops<emhash_map6>("emhash_map6");
    test_block<emhash_map6>("emhash_map6");
    test_block3<emhash_map6>("emhash_map6");

    test_loops<rig_hashmap>( "rigtorp::hashmap" );
    test_block<rig_hashmap>( "rigtorp::hashmap" );
    test_block3<rig_hashmap>( "rigtorp::hashmap" );

#if CK_HMAP
    test_loops<ck_hashmap>( "ck::hashmap" );
    test_block<ck_hashmap>( "ck::hashmap" );
#endif

    test_loops<boost_unordered_flat_map>( "boost::flat_hashmap" );
    test_block<boost_unordered_flat_map>("boost::flat_hashmap");
    test_block3<boost_unordered_flat_map>("boost::flat_hashmap");

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
#endif

    return 0;
}
