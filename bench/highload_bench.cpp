// Extreme high load factor benchmark for emhash
// Tests insert + erase stability at 0.999 load factor
// Restored from original RunHighLoadFactor() code

#include "util.h"
#include "../hash_table7.hpp"
#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table8.hpp"

template<typename HashT>
static void RunHighLoadFactorT(const char* name)
{
    std::random_device rd;
    const auto rand_key = rd();
    WyRand srngi(rand_key), srnge(rand_key);

    const auto max_lf   = 0.999f;
    const auto vsize    = 1u << 20; // 1M buckets
    HashT myhash(vsize, max_lf);

    auto nowus = getus();
    for (size_t i = 0; i < size_t(vsize * max_lf); i++)
        myhash.emplace(srngi(), (int)i);
    const auto insert_time = getus() - nowus;

    // verify no rehash
    if (myhash.bucket_count() != vsize)
        printf("%s: WARNING rehash occurred! bucket_count=%d vs vsize=%d\n", name, (int)myhash.bucket_count(), vsize);

    nowus = getus();
    //erase & insert at a fixed load factor
    for (size_t i = 0; i < vsize; i++) {
        myhash.erase(srnge()); //erase an old key
        myhash[srngi()] = 1;   //insert a new key
    }
    const auto erase_time = getus() - nowus;
    printf("%-16s vsize=%d, LF=%.4f, insert=%ldms, erase+insert=%ldms\n",
        name, vsize, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
}

// Benchmark find hit/miss at extreme high load factor
template<typename HashT>
static void RunHighLoadFindT(const char* name)
{
    std::random_device rd;
    const auto rand_key = rd();
    WyRand srngi(rand_key), srngf(rand_key + 1);

    const auto max_lf = 0.999f;
    const auto vsize  = 1u << 20;

    HashT myhash(vsize, max_lf);

    //fill to max_lf
    std::vector<int64_t> keys;
    keys.reserve(vsize);
    for (size_t i = 0; i < size_t(vsize * max_lf); i++) {
        auto k = srngi();
        keys.push_back(k);
        myhash.emplace(k, (int)i);
    }

    //find hit
    auto nowus = getus();
    volatile size_t sum = 0;
    for (auto& k : keys)
        sum += myhash.count(k);
    const auto fhit_time = getus() - nowus;

    //find miss
    nowus = getus();
    volatile size_t miss_sum = 0;
    for (size_t i = 0; i < keys.size(); i++)
        miss_sum += myhash.count(srngf());
    const auto fmiss_time = getus() - nowus;

    //iterate
    nowus = getus();
    volatile size_t iter_sum = 0;
    for (auto& p : myhash)
        iter_sum += p.second;
    const auto iter_time = getus() - nowus;

    printf("%-16s LF=%.4f, fhit=%ldms, fmiss=%ldms, iter=%ldms\n",
        name, myhash.load_factor(), fhit_time / 1000, fmiss_time / 1000, iter_time / 1000);
}

// Benchmark at different scales
static void RunHighLoadFactorScale()
{
    std::random_device rd;
    const auto rand_key = rd();

    for (int shift = 16; shift <= 24; shift += 4) {
        WyRand srngi(rand_key), srnge(rand_key);

        const auto max_lf = 0.999f;
        const auto vsize  = 1u << shift;
        emhash7::HashMap<int64_t, int> myhash(vsize, max_lf);

        auto nowus = getus();
        for (size_t i = 0; i < size_t(vsize * max_lf); i++)
            myhash.emplace(srngi(), (int)i);
        const auto insert_time = getus() - nowus; nowus = getus();

        for (size_t i = 0; i < vsize; i++) {
            myhash.erase(srnge());
            myhash[srngi()] = 1;
        }
        const auto erase_time = getus() - nowus;

        printf("emhash7: vsize=%d(2^%d), LF=%.4f, insert=%ldms, erase+insert=%ldms\n",
            vsize, shift, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
    }
}

// Original RunHighLoadFactor from old codebase
static void RunHighLoadFactor()
{
    std::random_device rd;
    const auto rand_key = rd();
#if 1
    WyRand srngi(rand_key), srnge(rand_key);
#else
    std::mt19937_64 srngi(rand_key), srnge(rand_key);
#endif

    const auto max_lf   = 0.999f; //<= 0.9999f
    const auto vsize    = 1u << (20 + rand_key % 6);//must be power of 2
    emhash7::HashMap<int64_t, int> myhash(vsize, max_lf);

    auto nowus = getus();
    for (size_t i = 0; i < size_t(vsize * max_lf); i++)
        myhash.emplace(srngi(), i);
    assert(myhash.bucket_count() == vsize); //no rehash

    const auto insert_time = getus() - nowus; nowus = getus();
    //erase & insert at a fixed load factor
    for (size_t i = 0; i < vsize; i++) {
        myhash.erase(srnge()); //erase an old key
        myhash[srngi()] = 1;   //insert a new key
    }
    const auto erase_time = getus() - nowus;
    printf("emhash7: vsize = %d, load factor = %.4f, insert/erase = %ld/%ld ms\n",
        vsize, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
    assert(myhash.load_factor() >= max_lf - 0.001);
}

int main()
{
    printf("=== Extreme High Load Factor (0.999) Benchmark ===\n\n");

    printf("--- Insert + Erase at LF=0.999 (1M buckets) ---\n");
    RunHighLoadFactorT<emhash7::HashMap<int64_t, int>>("emhash7");
    RunHighLoadFactorT<emhash6::HashMap<int64_t, int>>("emhash6");
    RunHighLoadFactorT<emhash5::HashMap<int64_t, int>>("emhash5");
    RunHighLoadFactorT<emhash8::HashMap<int64_t, int>>("emhash8");

    printf("\n--- Find Hit/Miss/Iter at LF=0.999 ---\n");
    RunHighLoadFindT<emhash7::HashMap<int64_t, int>>("emhash7");
    RunHighLoadFindT<emhash6::HashMap<int64_t, int>>("emhash6");
    RunHighLoadFindT<emhash5::HashMap<int64_t, int>>("emhash5");

    printf("\n--- Scale test at LF=0.999 (emhash7) ---\n");
    RunHighLoadFactorScale();

    printf("\n--- Original RunHighLoadFactor (random scale) ---\n");
    RunHighLoadFactor();

    return 0;
}
