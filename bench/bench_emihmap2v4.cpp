// Performance comparison: emhash8 vs emihmap2 vs emihmap4 vs Boost::unordered_flat_map
// emhash8:  split-index + dense-pairs (emhash main implementation)
// emihmap2: linear probing with bitmask + bucket index
// emihmap4: swiss-table style (group15, SSE2 byte matching, no tombstone)
// Boost:    swiss-table FOA (group15, single-block alloc, recover_slot anti-drift)
//
// Compile (GCC):   g++ -std=c++17 -O3 -march=native -I../include -I../thirdparty bench_emihmap2v4.cpp -o bench_4way
// Compile (Clang): clang++ -std=c++17 -O3 -march=native -I../include -I../thirdparty bench_emihmap2v4.cpp -o bench_4way

#include "emhash/hash_table8.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap4.hpp"

#include <boost/unordered/unordered_flat_map.hpp>

#include <chrono>
#include <random>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <algorithm>

using namespace std;

// ============================================================================
// Timing
// ============================================================================

template <typename Func>
double measure_ms(Func f, int iters = 1) {
    auto t0 = chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; i++) f();
    auto t1 = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(t1 - t0).count() / iters;
}

// ============================================================================
// Key generators
// ============================================================================

static vector<int64_t> gen_int_keys(size_t N, mt19937_64& rng) {
    vector<int64_t> keys(N);
    for (size_t i = 0; i < N; i++)
        keys[i] = static_cast<int64_t>(rng());
    return keys;
}

static vector<string> gen_string_keys(size_t N, mt19937_64& rng) {
    vector<string> keys(N);
    for (size_t i = 0; i < N; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "key_%016llx", (unsigned long long)rng());
        keys[i] = buf;
    }
    return keys;
}

template <typename K>
vector<K> gen_lookup_keys(const vector<K>& inserted, size_t M, mt19937_64& rng) {
    vector<K> look(M);
    for (size_t i = 0; i < M; i++) {
        if (i % 2 == 0) look[i] = inserted[rng() % inserted.size()];
        else {
            if constexpr (is_same_v<K, int64_t>)
                look[i] = static_cast<int64_t>(rng()) ^ 0xDEADBEEF;
            else
                look[i] = "miss_" + to_string(rng());
        }
    }
    return look;
}

// ============================================================================
// Core benchmark template
// ============================================================================

template <typename Map>
struct BenchResult {
    double insert_ns;
    double find_hit_ns;
    double find_miss_ns;
    double erase_ns;
    double iter_ns;
};

template <typename Map, typename K>
BenchResult<Map> run_bench(const vector<K>& keys, const vector<K>& look_hit, const vector<K>& look_miss) {
    BenchResult<Map> r{};
    size_t N = keys.size();
    size_t L = look_hit.size();

    Map m;
    m.reserve(N);

    // Insert
    auto t1 = measure_ms([&]() {
        for (auto& k : keys) m[k] = 1;
    });
    r.insert_ns = t1 * 1e6 / N;

    // Find hit
    {
        volatile size_t hits = 0;
        auto t2 = measure_ms([&]() {
            for (size_t i = 0; i < L; i++) {
                auto it = m.find(look_hit[i]);
                if (it != m.end()) hits++;
            }
        });
        r.find_hit_ns = t2 * 1e6 / L;
    }

    // Find miss
    {
        volatile size_t hits = 0;
        auto t3 = measure_ms([&]() {
            for (size_t i = 0; i < look_miss.size(); i++) {
                auto it = m.find(look_miss[i]);
                if (it != m.end()) hits++;
            }
        });
        r.find_miss_ns = t3 * 1e6 / look_miss.size();
    }

    // Iteration
    {
        volatile size_t sum = 0;
        auto t4 = measure_ms([&]() {
            for (auto& [k, v] : m) { (void)k; sum += v; }
        }, 10);
        r.iter_ns = t4 * 1e6 / (N * 10);
    }

    // Erase
    {
        size_t erase_n = N / 2;
        auto t5 = measure_ms([&]() {
            for (size_t i = 0; i < erase_n; i++) m.erase(keys[i]);
        });
        r.erase_ns = t5 * 1e6 / erase_n;
    }

    return r;
}

// ============================================================================
// Report helpers — 4-way comparison
// ============================================================================

static auto fmt_delta = [](double d) -> string {
    char buf[16];
    snprintf(buf, sizeof(buf), "%+.1f%%", d);
    return buf;
};

static void print_header4(const char* title) {
    cout << "\n┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────┐\n";
    cout << "│ " << left << setw(110) << title << "│\n";
    cout << "├──────────────┬─────────────┬─────────────┬─────────────┬─────────────┬──────────┬──────────┬──────────┐\n";
    cout << "│   Operation  │  emhash8    │  emihmap2   │  emihmap4   │   Boost     │v4 vs em8 │v4 vs em2 │Bo vs v4  │\n";
    cout << "├──────────────┼─────────────┼─────────────┼─────────────┼─────────────┼──────────┼──────────┼──────────┤\n";
}

static void print_row4(const char* op, double ns8, double ns2, double ns4, double nsB) {
    double d48 = (ns4 - ns8) / ns8 * 100.0;
    double d42 = (ns4 - ns2) / ns2 * 100.0;
    double dB4 = (nsB - ns4) / ns4 * 100.0;
    cout << "│ " << left << setw(12) << op
         << " │ " << right << setw(9) << fixed << setprecision(1) << ns8 << " ns"
         << " │ " << setw(9) << ns2 << " ns"
         << " │ " << setw(9) << ns4 << " ns"
         << " │ " << setw(9) << nsB << " ns"
         << " │ " << setw(8) << fmt_delta(d48)
         << " │ " << setw(8) << fmt_delta(d42)
         << " │ " << setw(8) << fmt_delta(dB4) << " │\n";
}

static void print_footer4() {
    cout << "└──────────────┴─────────────┴─────────────┴─────────────┴─────────────┴──────────┴──────────┴──────────┘\n";
}

// ============================================================================
// Run a 4-way benchmark for one key type at one size
// ============================================================================

template <typename K>
void bench_four_way(const char* key_label, size_t N,
                    const vector<K>& keys, const vector<K>& look_hit, const vector<K>& look_miss) {
    using Map8 = emhash8::HashMap<K, int>;
    using Map2 = emilib2::HashMap<K, int>;
    using Map4 = emilib4::HashMap<K, int>;
    using MapB = boost::unordered_flat_map<K, int>;

    auto r8 = run_bench<Map8>(keys, look_hit, look_miss);
    auto r2 = run_bench<Map2>(keys, look_hit, look_miss);
    auto r4 = run_bench<Map4>(keys, look_hit, look_miss);
    auto rB = run_bench<MapB>(keys, look_hit, look_miss);

    char title[128];
    snprintf(title, sizeof(title), "%s Keys  N=%zu", key_label, N);
    print_header4(title);
    print_row4("Insert",    r8.insert_ns,   r2.insert_ns,   r4.insert_ns,   rB.insert_ns);
    print_row4("FindHit",   r8.find_hit_ns, r2.find_hit_ns, r4.find_hit_ns, rB.find_hit_ns);
    print_row4("FindMiss",  r8.find_miss_ns,r2.find_miss_ns,r4.find_miss_ns,rB.find_miss_ns);
    print_row4("Erase",     r8.erase_ns,    r2.erase_ns,    r4.erase_ns,    rB.erase_ns);
    print_row4("Iterate",   r8.iter_ns,     r2.iter_ns,     r4.iter_ns,     rB.iter_ns);
    print_footer4();
}

// ============================================================================
// Sequential key benchmark
// ============================================================================

void bench_sequential_int() {
    const size_t N = 1000000;
    vector<int64_t> keys(N);
    for (size_t i = 0; i < N; i++) keys[i] = static_cast<int64_t>(i);

    using Map8 = emhash8::HashMap<int64_t, int>;
    using Map2 = emilib2::HashMap<int64_t, int>;
    using Map4 = emilib4::HashMap<int64_t, int>;
    using MapB = boost::unordered_flat_map<int64_t, int>;

    Map8 m8; m8.reserve(N);
    Map2 m2; m2.reserve(N);
    Map4 m4; m4.reserve(N);
    MapB mB; mB.reserve(N);

    auto ti8 = measure_ms([&](){ for (auto k : keys) m8[k] = 1; });
    auto ti2 = measure_ms([&](){ for (auto k : keys) m2[k] = 1; });
    auto ti4 = measure_ms([&](){ for (auto k : keys) m4[k] = 1; });
    auto tiB = measure_ms([&](){ for (auto k : keys) mB[k] = 1; });

    volatile size_t h8 = 0, h2 = 0, h4 = 0, hB = 0;
    auto tf8 = measure_ms([&](){ for (auto k : keys) if (m8.find(k) != m8.end()) h8++; });
    auto tf2 = measure_ms([&](){ for (auto k : keys) if (m2.find(k) != m2.end()) h2++; });
    auto tf4 = measure_ms([&](){ for (auto k : keys) if (m4.find(k) != m4.end()) h4++; });
    auto tfB = measure_ms([&](){ for (auto k : keys) if (mB.find(k) != mB.end()) hB++; });

    vector<int64_t> miss_keys(N);
    for (size_t i = 0; i < N; i++) miss_keys[i] = static_cast<int64_t>(i + N * 100);
    auto fm8 = measure_ms([&](){ volatile size_t h=0; for (auto k : miss_keys) if (m8.find(k) != m8.end()) h++; });
    auto fm2 = measure_ms([&](){ volatile size_t h=0; for (auto k : miss_keys) if (m2.find(k) != m2.end()) h++; });
    auto fm4 = measure_ms([&](){ volatile size_t h=0; for (auto k : miss_keys) if (m4.find(k) != m4.end()) h++; });
    auto fmB = measure_ms([&](){ volatile size_t h=0; for (auto k : miss_keys) if (mB.find(k) != mB.end()) h++; });

    size_t half = N / 2;
    auto te8 = measure_ms([&](){ for (size_t i = 0; i < half; i++) m8.erase(keys[i]); });
    auto te2 = measure_ms([&](){ for (size_t i = 0; i < half; i++) m2.erase(keys[i]); });
    auto te4 = measure_ms([&](){ for (size_t i = 0; i < half; i++) m4.erase(keys[i]); });
    auto teB = measure_ms([&](){ for (size_t i = 0; i < half; i++) mB.erase(keys[i]); });

    print_header4("Sequential int64_t Keys (Cache-Friendly)  N=1M");
    print_row4("Insert",   ti8*1e6/N, ti2*1e6/N, ti4*1e6/N, tiB*1e6/N);
    print_row4("FindHit",  tf8*1e6/N, tf2*1e6/N, tf4*1e6/N, tfB*1e6/N);
    print_row4("FindMiss", fm8*1e6/N, fm2*1e6/N, fm4*1e6/N, fmB*1e6/N);
    print_row4("Erase",    te8*1e6/half, te2*1e6/half, te4*1e6/half, teB*1e6/half);
    print_footer4();
}

// ============================================================================
// Mixed workload benchmark
// ============================================================================

void bench_mixed() {
    const size_t N = 500000;
    mt19937_64 rng(42);
    auto keys = gen_int_keys(N, rng);

    using Map8 = emhash8::HashMap<int64_t, int>;
    using Map2 = emilib2::HashMap<int64_t, int>;
    using Map4 = emilib4::HashMap<int64_t, int>;
    using MapB = boost::unordered_flat_map<int64_t, int>;

    Map8 m8; m8.reserve(N);
    Map2 m2; m2.reserve(N);
    Map4 m4; m4.reserve(N);
    MapB mB; mB.reserve(N);
    for (auto k : keys) { m8[k] = 1; m2[k] = 1; m4[k] = 1; mB[k] = 1; }

    mt19937_64 rng8(42);
    auto mix8 = measure_ms([&]() {
        for (size_t i = 0; i < N; i++) {
            auto op = rng8() % 100;
            if (op < 60) (void)m8.find(keys[rng8() % N]);
            else if (op < 85) m8[static_cast<int64_t>(rng8())] = 1;
            else m8.erase(keys[rng8() % (N/2)]);
        }
    });

    mt19937_64 rng2(42);
    auto mix2 = measure_ms([&]() {
        for (size_t i = 0; i < N; i++) {
            auto op = rng2() % 100;
            if (op < 60) (void)m2.find(keys[rng2() % N]);
            else if (op < 85) m2[static_cast<int64_t>(rng2())] = 1;
            else m2.erase(keys[rng2() % (N/2)]);
        }
    });

    mt19937_64 rng4(42);
    auto mix4 = measure_ms([&]() {
        for (size_t i = 0; i < N; i++) {
            auto op = rng4() % 100;
            if (op < 60) (void)m4.find(keys[rng4() % N]);
            else if (op < 85) m4[static_cast<int64_t>(rng4())] = 1;
            else m4.erase(keys[rng4() % (N/2)]);
        }
    });

    mt19937_64 rngB(42);
    auto mixB = measure_ms([&]() {
        for (size_t i = 0; i < N; i++) {
            auto op = rngB() % 100;
            if (op < 60) (void)mB.find(keys[rngB() % N]);
            else if (op < 85) mB[static_cast<int64_t>(rngB())] = 1;
            else mB.erase(keys[rngB() % (N/2)]);
        }
    });

    print_header4("Mixed Workload (60% find, 25% insert, 15% erase)  N=500K");
    print_row4("MixedOps", mix8*1e6/N, mix2*1e6/N, mix4*1e6/N, mixB*1e6/N);
    print_footer4();
}

// ============================================================================
// Main
// ============================================================================

int main() {
    cout << "========================================================================================\n";
    cout << "  emhash8 vs emihmap2 vs emihmap4 vs Boost  —  Four-Way Comparison\n";
    cout << "  Compiler: ";
#if defined(_MSC_VER)
    cout << "MSVC " << _MSC_VER;
#elif defined(__clang__)
    cout << "Clang " << __clang_major__ << "." << __clang_minor__;
#elif defined(__GNUC__)
    cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__;
#else
    cout << "Unknown";
#endif
    cout << "\n========================================================================================\n";
    cout << "  Columns: v4 vs em8 = emihmap4 vs emhash8;  v4 vs em2 = emihmap4 vs emihmap2\n";
    cout << "           Bo vs v4 = Boost vs emihmap4\n";
    cout << "  Negative = column header is faster; Positive = column header is slower\n";

    // ---- int64_t keys ----
    vector<size_t> int_sizes = {1000, 10000, 100000, 1000000};
    for (size_t N : int_sizes) {
        mt19937_64 rng(12345);
        auto keys = gen_int_keys(N, rng);
        auto look_hit = gen_lookup_keys(keys, N, rng);
        vector<int64_t> look_miss(N);
        mt19937_64 rng3(99999);
        for (size_t i = 0; i < N; i++) look_miss[i] = static_cast<int64_t>(rng3()) ^ 0xDEADBEEF;
        bench_four_way("Random int64_t", N, keys, look_hit, look_miss);
    }

    // ---- string keys ----
    vector<size_t> str_sizes = {1000, 10000, 100000, 500000};
    for (size_t N : str_sizes) {
        mt19937_64 rng(54321);
        auto keys = gen_string_keys(N, rng);
        auto look_hit = gen_lookup_keys(keys, N, rng);
        vector<string> look_miss(N);
        mt19937_64 rng3(77777);
        for (size_t i = 0; i < N; i++) look_miss[i] = "miss_" + to_string(rng3());
        bench_four_way("Random string", N, keys, look_hit, look_miss);
    }

    // Sequential key benchmark
    bench_sequential_int();

    // Mixed workload
    bench_mixed();

    return 0;
}
