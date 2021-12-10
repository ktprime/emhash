#include <inttypes.h>
#include <string>
#include "emilib/emilib2.hpp"

static inline uint64_t hashfib(uint64_t key)
{
#if __SIZEOF_INT128__
    __uint128_t r =  (__int128)key * UINT64_C(11400714819323198485);
    return (uint64_t)(r >> 64) + (uint64_t)r;
#elif _WIN64
    uint64_t high;
    return _umul128(key, UINT64_C(11400714819323198485), &high) + high;
#else
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#endif
}

static inline uint64_t hashmix(uint64_t key)
{
    auto ror  = (key >> 32) | (key << 32);
    auto low  = key * 0xA24BAED4963EE407ull;
    auto high = ror * 0x9FB21C651E98DF25ull;
    auto mix  = low + high;
    return mix;// (mix >> 32) | (mix << 32);
}

static inline uint64_t rrxmrrxmsx_0(uint64_t v)
{
    /* Pelle Evensen's mixer, https://bit.ly/2HOfynt */
    v ^= (v << 39 | v >> 25) ^ (v << 14 | v >> 50);
    v *= UINT64_C(0xA24BAED4963EE407);
    v ^= (v << 40 | v >> 24) ^ (v << 15 | v >> 49);
    v *= UINT64_C(0x9FB21C651E98DF25);
    return v ^ v >> 28;
}

static inline uint64_t hash_mur3(uint64_t key)
{
    //MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return h;
}

template<typename T>
struct Int64Hasher
{
    static constexpr uint64_t KC = UINT64_C(11400714819323198485);
    inline std::size_t operator()(T key) const
    {
#if FIB_HASH == 1
        return key;
#elif FIB_HASH == 2
        return hashfib(key);
#elif FIB_HASH == 3
        return hash_mur3(key);
#elif FIB_HASH == 4
        return hashmix(key);
#elif FIB_HASH == 5
        return rrxmrrxmsx_0(key);
#elif FIB_HASH > 10000
        return key % FIB_HASH; //bad hash
#elif FIB_HASH == 6
        return wyhash64(key, KC);
#else
        auto x = key;
        x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
        x = x ^ (x >> 31);
        return x;
#endif
    }
};

#if FIB_HASH
typedef emilib2::HashMap<int64_t, int64_t, Int64Hasher<int64_t>> hash_t;
#else
typedef emilib2::HashMap<int64_t, int64_t,  std::hash<int64_t>> hash_t;
#endif
typedef emilib2::HashMap<std::string, int64_t, std::hash<std::string>> str_hash_t;

