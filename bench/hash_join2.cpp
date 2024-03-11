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
#include <omp.h>

#if CK_HMAP
#include "util.h"
#include "ck/Common/HashTable/HashMap.h"
#endif

using namespace std::chrono_literals;
#if TKey == 0
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

constexpr float    max_lf = 0.60f;
uint32_t HASH_MEM_SIZE = 512 << 10;
constexpr uint32_t HASH_MAPS  = 1024;
constexpr uint32_t BLOCK_SIZE = 64;

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

    //#pragma omp parallel for schedule(static, 10240)
    for (int i = n1; i < n2; i++) {
        indices2[i] = rng();
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    //std::shuffle(indices2.begin(), indices2.end(), gen);

    auto t1 = std::chrono::steady_clock::now();
    printf("left join  size = %zd, memory = %zd MB, hash blocks = %d\n", indices1.size(), indices1.size() * sizeof(KeyType) >> 20, HASH_MAPS);
    printf("right join size = %zd, memory = %zd MB, init rand data time use %ld ms\n\n", indices2.size(), indices2.size() * sizeof(KeyType) >> 20, (t1 - t0) / 1ms);
}

template<template<class...> class Map>  void test_loops(char const* label)
{
    auto t0 = std::chrono::steady_clock::now();
    Map<KeyType, ValType> map(indices1.size() / 2);
    map.max_load_factor(max_lf);
    for (const auto v : indices1) {
        map.emplace(v, (ValType)v);
    }

    auto t1 = std::chrono::steady_clock::now();

    size_t ans = 0;
    #pragma omp parallel for num_threads(8) reduction(+:ans)
    for (int i = 0; i < indices2.size(); i++)
        ans += map.count(indices2[i]);

    auto tN = std::chrono::steady_clock::now();
    printf("%20s build %4zd ms, probe %4zd ms, lf = %.2f loops = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, map.load_factor(), ans);
}

template<template<class...> class Map>  void test_block( char const* label )
{
    auto t0 = std::chrono::steady_clock::now();
   //TODO: find a prime number or pow of 2
    auto hash_size = 1 + indices1.size() * sizeof(std::pair<KeyType,ValType>) / HASH_MEM_SIZE;
    hash_size = hash_size / 8 * 8 + 8;
    if (hash_size > HASH_MAPS)
        hash_size = HASH_MAPS;

//    constexpr auto hash_size = HASH_MAPS;
    Map<KeyType, ValType> map[HASH_MAPS];

#if 1
    std::vector<KeyType> arr1[HASH_MAPS];
    for (int i = 0; i < hash_size; i++)
        arr1[i].reserve(indices1.size() / hash_size * 11 / 10);
    for (const auto v:indices1)
        arr1[v % hash_size].emplace_back(v);

    #pragma omp parallel for num_threads(8)
    for (int i = 0; i < hash_size; i++) {
        auto& mapi = map[i];
        mapi.reserve(arr1[i].size());
        mapi.max_load_factor(max_lf);
        for (const auto v: arr1[i])
            mapi.emplace(v, (ValType)i);//row id
        arr1[i].clear();
        //mapi.clear();
    }
#else
    for (int i = 0; i < hash_size; i++) {
        auto& mapi = map[i];
        mapi.reserve(indices1.size() / hash_size);
        mapi.max_load_factor(max_lf);
    }
    for (const auto v:indices1)
        map[v % hash_size].emplace(v, (ValType)v);
#endif

    auto t1 = std::chrono::steady_clock::now();
    size_t ans = 0;
#if 0
    std::array<std::array<KeyType, BLOCK_SIZE>, HASH_MAPS> vblocks = {0};
    for (const auto v2:indices2) {
        const uint32_t bindex = v2 % hash_size;// (vhash & capacity) % HASH_MAPS; // save hash
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
#elif 0
    #pragma omp parallel reduction(+:ans)
    {
        #pragma omp for
        for (int i = 0; i < indices2.size(); i++) {
            const auto v = indices2[i];
            ans += map[v % hash_size].count(v);
        }
    }
#elif 1
    constexpr int threads = 8;
    const auto hash_threads = hash_size / threads;
    #pragma omp parallel num_threads(threads)
    {
        const int thread_id = omp_get_thread_num();
        auto ansi = 0;
        for (int i = 0; i < indices2.size(); i++) {
            const auto v = indices2[i];
            const auto idx = v % hash_size;
            if (idx / hash_threads == thread_id) {
                ansi += map[idx].count(v);
            }
        }
        ans += ansi;
    }
#else
//    std::vector<KeyType> arr2[HASH_MAPS];
//    for (int i = 0; i < HASH_MAPS; i++)
//        arr1[i].reserve(indices2.size() / HASH_MAPS * 11 / 10);
    for (const auto v:indices2)
        arr1[v % hash_size].emplace_back(v); //keep row id

    #pragma omp parallel for num_threads(8) reduction(+:ans)
    for (int i = 0; i < hash_size; i++) {
        auto& mapi = map[i];
        auto ansi = 0;
        for (const auto v: arr1[i])
            ansi += mapi.count(v);
        ans += ansi;
        arr1[i].clear();
        //mapi.clear();
    }
#endif

    auto tN = std::chrono::steady_clock::now();
    printf("%20s build %4zd ms, probe %4zd ms, mem = %4ld hash_size = %zd, ans = %zd\n\n",
            label, (t1 - t0) / 1ms, (tN - t1) / 1ms, map[0].bucket_count() * sizeof(std::pair<KeyType,ValType>) / 1024, hash_size, ans);
}

template<template<class...> class Map>  void test_block2( char const* label )
{
#if 0
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
    printf("%20s build %4zd ms, probe %4zd ms, join_block2 = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
#endif
}

template<template<class...> class Map>  void test_block3( char const* label )
{
#if 0
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
    printf("%20s build %4zd ms, probe %4zd ms, join_block2 = %zd\n", label, (t1 - t0) / 1ms, (tN - t1) / 1ms, ans);
#endif
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
    if (argc > 4 && isdigit(argv[4][0])) HASH_MEM_SIZE = atoi(argv[4]) * 1024;

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
