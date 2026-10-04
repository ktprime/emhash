// bench/ht8_optimize_bench.cpp
// A/B benchmark for the hash_table8.hpp optimizations borrowed from
// martinus/unordered_dense (Aug-Sep 2026 commits):
//   1. shared empty-index sentinel + lazy allocation (empty map: 0 allocations)
//   2. store-forwarding-friendly wyhash for fixed-size 8/12/16-byte POD keys
//      (build with -DEMH_POD_HASH=1)
//   3. ADL-findable swap (generic swap() no longer falls into std::swap)
//   4. precomputed-hash lookup overloads (guarded by EMH_HASH_LOOKUP)
//
// Same source compiles against the ORIGINAL header and the OPTIMIZED header;
// select with include-path order:
//   orig: -I <shadow-orig-dir> -I include ...
//   new : -I <shadow-new-dir> -I include ... [-DEMH_POD_HASH=1]
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "emhash/hash_table8.hpp"

// The ORIG header predates these macros; fall back so this source compiles against both.
#ifndef EMH_POD_HASH
#define EMH_POD_HASH 0
#endif

// ---------------------------------------------------------------------------
// allocation counting
// ---------------------------------------------------------------------------
static long g_allocs = 0;
static long g_frees = 0;

template <typename T> struct counting_allocator {
    using value_type = T;
    counting_allocator() = default;
    template <typename U> counting_allocator(const counting_allocator<U>&) noexcept {}
    T* allocate(size_t n) {
        ++g_allocs;
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    void deallocate(T* p, size_t) noexcept {
        ++g_frees;
        ::operator delete(p);
    }
};
template <typename T, typename U> bool operator==(const counting_allocator<T>&, const counting_allocator<U>&) noexcept {
    return true;
}
template <typename T, typename U> bool operator!=(const counting_allocator<T>&, const counting_allocator<U>&) noexcept {
    return false;
}

using AMap = emhash8::HashMap<int, int, std::hash<int>, std::equal_to<int>, counting_allocator<std::pair<int, int>>>;

// ---------------------------------------------------------------------------
// timing helpers
// ---------------------------------------------------------------------------
static volatile uint64_t g_sink = 0;

static double now_ns() {
    using clk = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(clk::now().time_since_epoch()).count();
}

// best-of-5 ns/op
template <typename Fn> static double bench_ns(size_t iters, Fn&& fn) {
    double best = 1e300;
    for (int rep = 0; rep < 5; ++rep) {
        const auto t0 = now_ns();
        fn();
        const auto t1 = now_ns();
        const double per = double(t1 - t0) / double(iters);
        if (per < best)
            best = per;
    }
    return best;
}

// best-of-5 ns/op where every rep gets a FRESH state from setup(), so mutating
// phases (insert/erase) measure their real cost and not a repeat no-op pass.
// setup() runs untimed.
template <typename Setup, typename Body> static double bench_phase(size_t iters, Setup&& setup, Body&& body) {
    double best = 1e300;
    for (int rep = 0; rep < 5; ++rep) {
        auto state = setup(); // fresh map / key set per rep
        const auto t0 = now_ns();
        body(state);
        const auto t1 = now_ns();
        const double per = double(t1 - t0) / double(iters);
        if (per < best)
            best = per;
    }
    return best;
}

static void report(const char* name, size_t iters, double ns, long allocs) {
    printf("  %-42s %10.2f ns/op  allocs=%ld\n", name, ns, allocs);
}

// ---------------------------------------------------------------------------
// keys
// ---------------------------------------------------------------------------
struct Coord12 { // no padding: 3 x int32
    int32_t x, y, z;
    bool operator==(const Coord12& r) const { return x == r.x && y == r.y && z == r.z; }
};
struct CoordHash { // the "before": two overlapping 8-byte loads spanning fields
    size_t operator()(const Coord12& c) const noexcept {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&c);
        uint64_t a, b;
        std::memcpy(&a, p, 8);
        std::memcpy(&b, p + 4, 8);
        a = a * UINT64_C(0x9E3779B97F4A7C15) ^ (b >> 31);
        a ^= a >> 33;
        a *= UINT64_C(0xff51afd7ed558ccd);
        a ^= a >> 33;
        return static_cast<size_t>(a);
    }
};

static uint64_t rng_state = 0x12345678ull;
static uint64_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

int main() {
    printf("== emhash8 optimization A/B bench (EMH_POD_HASH=%d, EMH_HASH_LOOKUP=%d) ==\n", EMH_POD_HASH,
#ifdef EMH_HASH_LOOKUP
           1
#else
           0
#endif
    );

    // ------------------------------------------------------------------
    // S1: default construction + destruction (lazy allocation)
    // ------------------------------------------------------------------
    {
        const size_t iters = 1000000;
        g_allocs = g_frees = 0;
        const double ns = bench_ns(iters, [&] {
            for (size_t i = 0; i < iters; ++i) {
                AMap m;
                g_sink += m.bucket_count();
            }
        });
        report("S1 default ctor+dtor", iters, ns, g_allocs);
    }
    {
        // empty-map lookups
        AMap m;
        const size_t iters = 10000000;
        const double ns = bench_ns(iters, [&] {
            for (size_t i = 0; i < iters; ++i)
                g_sink += m.contains((int)i);
        });
        report("S1 find() on empty map", iters, ns, 0);
    }

    // ------------------------------------------------------------------
    // S2: move construction ping-pong (move ctor is allocation-free now)
    // ------------------------------------------------------------------
    {
        const size_t iters = 1000000;
        AMap src;
        for (int i = 0; i < 64; ++i)
            src[i] = i;
        g_allocs = g_frees = 0;
        const double ns = bench_ns(iters, [&] {
            for (size_t i = 0; i < iters; ++i) {
                AMap dst = std::move(src);
                src = std::move(dst);
#if defined(__GNUC__) || defined(__clang__)
                __asm__ volatile("" ::: "memory"); // keep the moves observable
#endif
            }
        });
        report("S2 move-ctor ping-pong (64 elems)", iters, ns, g_allocs);
    }

    // ------------------------------------------------------------------
    // S3: generic swap() resolution (ADL swap)
    // ------------------------------------------------------------------
    {
        const size_t iters = 1000000;
        AMap a, b;
        for (int i = 0; i < 1000; ++i)
            a[i] = i;
        for (int i = 0; i < 10; ++i)
            b[i] = -i;
        g_allocs = g_frees = 0;
        const double ns = bench_ns(iters, [&] {
            for (size_t i = 0; i < iters; ++i) {
                using std::swap; // the way generic code writes it
                swap(a, b);
#if defined(__GNUC__) || defined(__clang__)
                __asm__ volatile("" ::: "memory"); // keep the swaps observable
#endif
            }
        });
        report("S3 std::swap-resolution swap (1k/10)", iters, ns, g_allocs);
    }

    // ------------------------------------------------------------------
    // S4: raw hash function of a 12-byte POD key (store forwarding)
    // S4a is the same manual hasher in both variants (the "before" function);
    // S4b is the map's own hash() over the key, which with -DEMH_POD_HASH=1
    // reads 4-byte words so a load never spans two of the three field stores.
    // ------------------------------------------------------------------
    {
        const size_t iters = 2000000;
        const double ns_a = bench_ns(iters, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < iters; ++i) {
                Coord12 c{(int32_t)i, (int32_t)(i * 3), (int32_t)(i * 7)}; // written field by field
                s += CoordHash{}(c);
            }
            g_sink += s;
        });
        report("S4a manual 8-byte-read hash", iters, ns_a, 0);
#ifdef EMH_HASH_LOOKUP
        emhash8::HashMap<Coord12, int, CoordHash> hm;
        const double ns_b = bench_ns(iters, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < iters; ++i) {
                Coord12 c{(int32_t)i, (int32_t)(i * 3), (int32_t)(i * 7)};
                s += hm.hash(c);
            }
            g_sink += s;
        });
        report("S4b map hash() (EMH_POD_HASH path)", iters, ns_b, 0);
#endif
    }

    // ------------------------------------------------------------------
    // S5: map<Coord12> insert / find-hit / find-miss (EMH_POD_HASH matters)
    // ------------------------------------------------------------------
    for (size_t N : {size_t(65536), size_t(1000000)}) {
        std::vector<Coord12> keys(N);
        for (size_t i = 0; i < N; ++i) {
            const uint64_t r = rng();
            keys[i] = Coord12{(int32_t)r, (int32_t)(r >> 24), (int32_t)(r >> 48)};
        }
        using CMap = emhash8::HashMap<Coord12, int, CoordHash>;
        CMap m;
        m.reserve(N);
        const double ns_ins = bench_ns(N, [&] {
            for (size_t i = 0; i < N; ++i) {
                const Coord12 c = keys[i]; // copy: key written just before lookup
                m[c] = (int)i;
            }
        });
        char name[64];
        snprintf(name, sizeof(name), "S5 insert (N=%zu)", N);
        report(name, N, ns_ins, 0);

        const double ns_hit = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i) {
                const Coord12 c = keys[i]; // written right before the lookup
                auto it = m.find(c);
                s += it != m.end();
            }
            g_sink += s;
        });
        snprintf(name, sizeof(name), "S5 find-hit (N=%zu)", N);
        report(name, N, ns_hit, 0);

        const double ns_miss = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i) {
                Coord12 c = keys[i];
                c.x += 7; // absent
                auto it = m.find(c);
                s += it != m.end();
            }
            g_sink += s;
        });
        snprintf(name, sizeof(name), "S5 find-miss (N=%zu)", N);
        report(name, N, ns_miss, 0);
    }

    // ------------------------------------------------------------------
    // S6: string lookups with precomputed hash (EMH_HASH_LOOKUP)
    // ------------------------------------------------------------------
    {
        const size_t N = 65536;
        std::vector<std::string> keys;
        keys.reserve(N);
        for (size_t i = 0; i < N; ++i)
            keys.push_back("user_" + std::to_string(i * 7)); // SSO
        emhash8::HashMap<std::string, int> m;
        m.reserve(N);
        for (size_t i = 0; i < N; ++i)
            m[keys[i]] = (int)i;

#ifdef EMH_HASH_LOOKUP
        std::vector<uint64_t> hs(N);
        for (size_t i = 0; i < N; ++i)
            hs[i] = m.hash(keys[i]);
        std::vector<std::string> absent;
        std::vector<uint64_t> ahs;
        absent.reserve(1024);
        ahs.reserve(1024);
        for (size_t i = 0; i < 1024; ++i) {
            absent.emplace_back("absent_" + std::to_string(i * 7));
            ahs.push_back(m.hash(absent.back()));
        }
        const double ns_hit = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(keys[i], hs[i]) != m.end();
            g_sink += s;
        });
        report("S6 find-hit precomputed hash (SSO)", N, ns_hit, 0);
        const double ns_miss = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.contains(absent[i & 1023], ahs[i & 1023]) ? 1 : 0;
            g_sink += s;
        });
        report("S6 find-miss precomputed hash (SSO)", N, ns_miss, 0);
#endif
        // plain find: hashes inside
        const double ns_hit_plain = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(keys[i]) != m.end();
            g_sink += s;
        });
        report("S6 find-hit hash-inside (SSO)", N, ns_hit_plain, 0);
    }

    // ------------------------------------------------------------------
    // S7: REGULAR int-key workload at the default build config — the
    // no-regression check for find / insert / erase / churn hot paths
    // ------------------------------------------------------------------
    for (size_t N : {size_t(65536), size_t(1000000)}) {
        std::vector<int> keys(N), miss(N), renew(N / 2);
        for (size_t i = 0; i < N; ++i) {
            keys[i] = (int)(rng() & 0x3fffffffu);
            miss[i] = (int)0x40000000u + (int)(rng() & 0x3fffffffu);
            if (i < N / 2)
                renew[i] = (int)0x60000000u + (int)(rng() & 0x3fffffffu);
        }
        using IMap = emhash8::HashMap<int, int>;
        char name[64];

        const auto fill = [&](IMap& m) {
            for (size_t i = 0; i < N; ++i)
                m.emplace(keys[i], (int)i);
        };

        const double ns_ins = bench_phase(
            N, [&] { return IMap{}; },
            [&](IMap& m) {
                for (size_t i = 0; i < N; ++i)
                    m.emplace(keys[i], (int)i);
            });
        snprintf(name, sizeof(name), "S7 int insert (N=%zu)", N);
        report(name, N, ns_ins, 0);

        const double ns_hit = bench_phase(
            N,
            [&] {
                IMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](IMap& m) {
                uint64_t s = 0;
                for (size_t i = 0; i < N; ++i)
                    s += m.find(keys[i]) != m.end();
                g_sink += s;
            });
        snprintf(name, sizeof(name), "S7 int find-hit (N=%zu)", N);
        report(name, N, ns_hit, 0);

        const double ns_miss = bench_phase(
            N,
            [&] {
                IMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](IMap& m) {
                uint64_t s = 0;
                for (size_t i = 0; i < N; ++i)
                    s += m.find(miss[i]) != m.end();
                g_sink += s;
            });
        snprintf(name, sizeof(name), "S7 int find-miss (N=%zu)", N);
        report(name, N, ns_miss, 0);

        const double ns_ers = bench_phase(
            N,
            [&] {
                IMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](IMap& m) {
                for (size_t i = 0; i < N; ++i)
                    m.erase(keys[i]);
                g_sink += m.size();
            });
        snprintf(name, sizeof(name), "S7 int erase (N=%zu)", N);
        report(name, N, ns_ers, 0);

        const double ns_churn = bench_phase(
            N,
            [&] {
                IMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](IMap& m) {
                for (size_t i = 0; i < N / 2; ++i) { // N ops: N/2 erases + N/2 inserts
                    m.erase(keys[i * 2]);
                    m.emplace(renew[i], (int)i);
                }
                g_sink += m.size();
            });
        snprintf(name, sizeof(name), "S7 int churn (N=%zu)", N);
        report(name, N, ns_churn, 0);
    }

    // ------------------------------------------------------------------
    // S8: REGULAR string-key workload (heap ~20 chars, SSO <=15 chars)
    // ------------------------------------------------------------------
    for (size_t N : {size_t(65536), size_t(1000000)}) {
        std::vector<std::string> keys(N), miss(N), renew(N / 2);
        for (size_t i = 0; i < N; ++i) {
            keys[i] = "user_" + std::to_string(rng() % 100000000000ull); // heap (17-21 chars)
            miss[i] = "absent_" + std::to_string(rng() % 100000000000ull);
            if (i < N / 2)
                renew[i] = "fresh_" + std::to_string(rng() % 100000000000ull);
        }
        using SMap = emhash8::HashMap<std::string, int>;
        char name[64];

        const auto fill = [&](SMap& m) {
            for (size_t i = 0; i < N; ++i)
                m.emplace(keys[i], (int)i);
        };

        const double ns_ins = bench_phase(
            N, [&] { return SMap{}; },
            [&](SMap& m) {
                for (size_t i = 0; i < N; ++i)
                    m.emplace(keys[i], (int)i);
            });
        snprintf(name, sizeof(name), "S8 str insert (N=%zu)", N);
        report(name, N, ns_ins, 0);

        const double ns_hit = bench_phase(
            N,
            [&] {
                SMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](SMap& m) {
                uint64_t s = 0;
                for (size_t i = 0; i < N; ++i)
                    s += m.find(keys[i]) != m.end();
                g_sink += s;
            });
        snprintf(name, sizeof(name), "S8 str find-hit (N=%zu)", N);
        report(name, N, ns_hit, 0);

        const double ns_miss = bench_phase(
            N,
            [&] {
                SMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](SMap& m) {
                uint64_t s = 0;
                for (size_t i = 0; i < N; ++i)
                    s += m.find(miss[i]) != m.end();
                g_sink += s;
            });
        snprintf(name, sizeof(name), "S8 str find-miss (N=%zu)", N);
        report(name, N, ns_miss, 0);

        const double ns_ers = bench_phase(
            N,
            [&] {
                SMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](SMap& m) {
                for (size_t i = 0; i < N; ++i)
                    m.erase(keys[i]);
                g_sink += m.size();
            });
        snprintf(name, sizeof(name), "S8 str erase (N=%zu)", N);
        report(name, N, ns_ers, 0);

        const double ns_churn = bench_phase(
            N,
            [&] {
                SMap m;
                m.reserve(N);
                fill(m);
                return m;
            },
            [&](SMap& m) {
                for (size_t i = 0; i < N / 2; ++i) {
                    m.erase(keys[i * 2]);
                    m.emplace(renew[i], (int)i);
                }
                g_sink += m.size();
            });
        snprintf(name, sizeof(name), "S8 str churn (N=%zu)", N);
        report(name, N, ns_churn, 0);

        if (N == 65536) { // SSO variant, small size only
            std::vector<std::string> skeys(N);
            for (size_t i = 0; i < N; ++i)
                skeys[i] = "u" + std::to_string(i); // <=15 chars, SSO
            const double ns_sso = bench_phase(
                N,
                [&] {
                    SMap m;
                    m.reserve(N);
                    return m;
                },
                [&](SMap& m) {
                    for (size_t i = 0; i < N; ++i)
                        m.emplace(skeys[i], (int)i);
                });
            snprintf(name, sizeof(name), "S8 str-SSO insert (N=%zu)", N);
            report(name, N, ns_sso, 0);
            const double ns_ssohit = bench_phase(
                N,
                [&] {
                    SMap m;
                    m.reserve(N);
                    for (size_t i = 0; i < N; ++i)
                        m.emplace(skeys[i], (int)i);
                    return m;
                },
                [&](SMap& m) {
                    uint64_t s = 0;
                    for (size_t i = 0; i < N; ++i)
                        s += m.find(skeys[i]) != m.end();
                    g_sink += s;
                });
            snprintf(name, sizeof(name), "S8 str-SSO find-hit (N=%zu)", N);
            report(name, N, ns_ssohit, 0);
        }
    }

#ifdef EMH_HASH_LOOKUP
    // ------------------------------------------------------------------
    // S9: heterogeneous lookup -- std::string_view / const char* keys against
    // a std::string-keyed map (emhash8::string_hash + std::equal_to<>), versus
    // the temporary-std::string lookup they replace. The temporary pays a
    // string copy + heap allocation per lookup.
    // ------------------------------------------------------------------
    {
        const size_t N = 65536;
        using HMap = emhash8::HashMap<std::string, int, emhash8::string_hash, std::equal_to<>>;
        std::vector<std::string> keys(N), absent(N);
        for (size_t i = 0; i < N; ++i) {
            keys[i] = "user_" + std::to_string(i * 7);
            absent[i] = "nope_" + std::to_string(i * 7);
        }
        std::vector<std::string_view> svs(N), sva(N);
        for (size_t i = 0; i < N; ++i) {
            svs[i] = keys[i];
            sva[i] = absent[i];
        }
        HMap m;
        m.reserve(N);
        for (size_t i = 0; i < N; ++i)
            m.emplace(keys[i], (int)i);

        const double ns_sv = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(svs[i]) != m.end();
            g_sink += s;
        });
        report("S9 find-hit hetero (string_view)", N, ns_sv, 0);

        const double ns_cstr = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(keys[i].c_str()) != m.end();
            g_sink += s;
        });
        report("S9 find-hit hetero (const char*)", N, ns_cstr, 0);

        const double ns_tmp = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(std::string(svs[i])) != m.end();
            g_sink += s;
        });
        report("S9 find-hit temporary string", N, ns_tmp, 0);

        const double ns_miss = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(sva[i]) != m.end();
            g_sink += s;
        });
        report("S9 find-miss hetero (string_view)", N, ns_miss, 0);

        const double ns_tmpmiss = bench_ns(N, [&] {
            uint64_t s = 0;
            for (size_t i = 0; i < N; ++i)
                s += m.find(absent[i]) != m.end();
            g_sink += s;
        });
        report("S9 find-miss temporary string", N, ns_tmpmiss, 0);
    }
#endif

    printf("done (sink=%llu)\n", (unsigned long long)g_sink);
    return 0;
}
