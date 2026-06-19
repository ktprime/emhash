// Regression test for hash_table8.hpp / hash_set8.hpp find_slot_bucket
// self-loop check removal.
//
// Background:
//   Commit e292e5f added a defensive self-loop break in find_slot_bucket:
//     const auto nbucket = _index[next_bucket].next;
//     if (nbucket == next_bucket) break;
//     next_bucket = nbucket;
//
//   This costs 1 extra load + 1 extra compare per probe iteration
//   on the hot find/erase path (~1-2% regression).
//
// Invariant under test:
//   find_slot_bucket is only ever called for a `slot` that is known
//   to be present in the chain. In that case, the lookup always
//   returns successfully before the chain terminator is reached, so
//   the loop is guaranteed to terminate and the slot is found.
//
//   Callers (in this header) of find_slot_bucket:
//     - slot_to_bucket(): unconditionally returns its result; the
//       caller is erase_slot (see below) which only invokes it when
//       last_slot is known to exist in the table.
//     - erase_slot(): uses find_slot_bucket to relocate last_slot's
//       bucket when last_slot != slot. last_slot = _num_filled - 1
//       is always a valid, populated slot in the table.
//
//   The following tests aggressively exercise the code paths that
//   call find_slot_bucket (erase / slot_to_bucket) with adversarial
//   hash functions and high collision rates, and verify:
//     1. Operations complete (no infinite loop / hang).
//     2. All data is preserved correctly across many insert/erase
//        cycles, which transitively proves find_slot_bucket returns
//        the correct bucket for the slot it is asked to locate.

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cassert>

#include "emhash/hash_table8.hpp"
#include "emhash/hash_set8.hpp"

// =============================================================================
// Adversarial hashers that maximise chain length and exercise find_slot_bucket.
// =============================================================================

// All keys hash to the same bucket.  Forces the longest possible chains.
struct AllSameHash {
    size_t operator()(int) const { return 0xCAFEBABE; }
};

// Returns high bits of key, low bits == 0 -> all collide in bucket 0
// for any power-of-two bucket count.
struct ZeroLowBitsHash {
    size_t operator()(int key) const { return (size_t)key << 16; }
};

// Tiny range hash (only 16 distinct hash values for 32-bit key space).
struct TinyRangeHash {
    size_t operator()(int key) const { return (size_t)(key * 2654435761u) >> 28; }
};

// Multiplicative hash with prime that creates long probe chains.
struct PrimeChainHash {
    size_t operator()(int key) const { return (size_t)key * 0x9E3779B1u; }
};

// =============================================================================
// Test 1: Erase tail element under maximum collision
// =============================================================================
// find_slot_bucket is called from erase_slot() to relocate the last
// slot's bucket when it is not the same as the slot being erased.
// We trigger this by erasing the first element while many other
// elements collide in the same bucket, so the kickout/relink path
// has to call find_slot_bucket on _num_filled - 1.

template <typename Map, typename Hasher>
static int test_erase_triggers_find_slot(const char* name, int n) {
    printf("\n=== %s : n=%d ===\n", name, n);
    auto t0 = std::chrono::steady_clock::now();

    Map m;
    for (int i = 0; i < n; ++i) m.insert_unique(i, i * 7 + 1);

    // Erase in a pattern that forces many calls into find_slot_bucket
    // via erase_slot -> slot_to_bucket -> find_slot_bucket.
    // We erase the first element repeatedly: each erase triggers
    // "swap with last + dtor last" which calls find_slot_bucket on
    // last_slot.  Doing this n times exercises find_slot_bucket n
    // times with different last slots.
    for (int round = 0; round < n; ++round) {
        auto it = m.begin();
        if (it == m.end()) break;
        m.erase(it);
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  erase-front x%d: %d remaining, %.1fms\n", n,
           (int)m.size(), ms);
    if (!m.empty()) {
        printf("  FAIL: map should be empty after erasing front n times\n");
        return 1;
    }
    printf("  PASS\n");
    return 0;
}

// =============================================================================
// Test 2: Random insert/erase matches std::unordered_map reference
// =============================================================================
// Heavy insert/erase churn under adversarial hashing.  The reference
// map validates that all data is preserved exactly.  Hangs would
// indicate find_slot_bucket failing to terminate.

template <typename Map, typename Hasher>
static int test_random_churn(const char* name, int iters, unsigned seed) {
    printf("\n=== %s : iters=%d seed=%u ===\n", name, iters, seed);
    auto t0 = std::chrono::steady_clock::now();

    Map m;
    std::unordered_map<int, int> ref;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> keydist(0, 1 << 20);
    std::uniform_int_distribution<int> valdist(0, 1 << 28);
    std::uniform_int_distribution<int> opdist(0, 9);

    int insert_count = 0, erase_count = 0, lookup_count = 0;
    for (int i = 0; i < iters; ++i) {
        int op = opdist(rng);
        int k = keydist(rng);
        if (op < 5) {
            int v = valdist(rng);
            // operator[] matches std::unordered_map's [] semantics
            // (insert or update).  emhash8::insert_unique requires the
            // caller to guarantee key uniqueness and is intentionally
            // not checked here.
            m[k] = v;
            ref[k] = v;
            ++insert_count;
        } else if (op < 8) {
            m.erase(k);
            ref.erase(k);
            ++erase_count;
        } else {
            auto it = m.find(k);
            auto rit = ref.find(k);
            if ((it == m.end()) != (rit == ref.end())) {
                printf("  FAIL: key=%d existence mismatch at iter %d\n", k, i);
                return 1;
            }
            if (it != m.end() && it->second != rit->second) {
                printf("  FAIL: key=%d value %d != %d at iter %d\n",
                       k, it->second, rit->second, i);
                return 1;
            }
            ++lookup_count;
        }
    }

    // Final exhaustive verification.
    if (m.size() != ref.size()) {
        printf("  FAIL: size mismatch m=%zu ref=%zu\n",
               (size_t)m.size(), (size_t)ref.size());
        return 1;
    }
    for (auto& kv : ref) {
        auto it = m.find(kv.first);
        if (it == m.end() || it->second != kv.second) {
            printf("  FAIL: key=%d missing or wrong in final check\n",
                   kv.first);
            return 1;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  ins=%d era=%d lkup=%d size=%zu : %.1fms  PASS\n",
           insert_count, erase_count, lookup_count,
           (size_t)m.size(), ms);
    return 0;
}

// =============================================================================
// Test 3: Erase via iterator on a constant-hash table
// =============================================================================
// All keys share the same bucket, so every erase calls
// find_slot_bucket to relocate the last slot.  The chain length
// can be > 1,000.

template <typename Map, typename Hasher>
static int test_erase_iter_const_hash(const char* name, int n) {
    printf("\n=== %s : n=%d ===\n", name, n);
    auto t0 = std::chrono::steady_clock::now();

    Map m;
    for (int i = 0; i < n; ++i) m.insert_unique(i, i);

    // Iterate-and-erase a random half of the elements.
    std::mt19937 rng(42);
    std::vector<int> to_erase;
    for (int i = 0; i < n; ++i) to_erase.push_back(i);
    std::shuffle(to_erase.begin(), to_erase.end(), rng);

    int half = n / 2;
    for (int i = 0; i < half; ++i) {
        m.erase(to_erase[i]);
    }
    // Verify survivors.
    for (int i = half; i < n; ++i) {
        auto it = m.find(to_erase[i]);
        if (it == m.end() || it->second != to_erase[i]) {
            printf("  FAIL: survivor key=%d missing/wrong at iter %d\n",
                   to_erase[i], i);
            return 1;
        }
    }
    // Erased must be gone.
    for (int i = 0; i < half; ++i) {
        if (m.find(to_erase[i]) != m.end()) {
            printf("  FAIL: erased key=%d still present\n", to_erase[i]);
            return 1;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  size=%zu expected=%d : %.1fms  PASS\n",
           (size_t)m.size(), n - half, ms);
    return 0;
}

// =============================================================================
// Test 4: Concurrent erase from multiple threads
// =============================================================================
// NOTE: emhash8 is intentionally NOT thread-safe for writes (the API
// documents this).  Concurrent erase is therefore an invalid usage
// pattern and is not exercised here.  We still call this from main()
// with threads=1 to confirm the single-threaded erase path is fine
// and keep the test infrastructure in case future revisions add
// internal locking.

template <typename Map, typename Hasher>
static int test_concurrent_erase(const char* name, int n, int threads) {
    printf("\n=== %s : n=%d threads=%d ===\n", name, n, threads);
    auto t0 = std::chrono::steady_clock::now();

    Map* m = new Map();
    for (int i = 0; i < n; ++i) m->insert_unique(i, i);

    std::atomic<int> failures{0};
    std::vector<std::thread> ts;
    int per = n / threads;

    for (int t = 0; t < threads; ++t) {
        ts.emplace_back([m, t, per, n, threads, &failures]() {
            int lo = t * per;
            int hi = (t == threads - 1) ? n : (t + 1) * per;
            for (int k = lo; k < hi; ++k) {
                if (m->erase(k) != 1) failures.fetch_add(1);
            }
        });
    }
    for (auto& th : ts) th.join();

    int f = failures.load();
    if (f != 0) {
        printf("  FAIL: %d erase returned 0\n", f);
        delete m;
        return 1;
    }
    if (!m->empty()) {
        printf("  FAIL: map not empty (size=%zu)\n", (size_t)m->size());
        delete m;
        return 1;
    }
    delete m;
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  %.1fms  PASS\n", ms);
    return 0;
}

// =============================================================================
// Test 5: Erase the last iterator specifically (targets find_slot_bucket)
// =============================================================================
// m.last() returns an iterator to _pairs[_num_filled - 1].
// m.erase(it) -> erase_slot(last_bucket, last_main_bucket).
// erase_slot sees last_slot == slot, skips the find_slot_bucket
// branch, so this test does NOT exercise find_slot_bucket.
// We keep it to document behaviour.

template <typename Map, typename Hasher>
static int test_erase_last(const char* name, int n) {
    printf("\n=== %s : n=%d ===\n", name, n);
    Map m;
    for (int i = 0; i < n; ++i) m.insert_unique(i, i);

    while (!m.empty()) {
        auto it = m.last();
        int expected = it->second;
        auto next = m.erase(it);
        (void)next; (void)expected;
    }
    if (!m.empty()) {
        printf("  FAIL: not empty after erasing last repeatedly\n");
        return 1;
    }
    printf("  PASS\n");
    return 0;
}

// =============================================================================
// Test 6: Stress with hash_set8 (const KeyT) instead of HashMap
// =============================================================================
// Mirror the HashMap tests for HashSet to cover the duplicate
// find_slot_bucket site at hash_set8.hpp:1231-1236.

template <typename SetT, typename Hasher>
static int test_set_random_churn(const char* name, int iters, unsigned seed) {
    printf("\n=== %s : iters=%d seed=%u ===\n", name, iters, seed);
    auto t0 = std::chrono::steady_clock::now();

    SetT s;
    std::unordered_set<int> refset;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> keydist(0, 1 << 20);
    std::uniform_int_distribution<int> opdist(0, 9);

    // Track keys that are already present so we use insert() only
    // when we know the key is unique (matches emhash semantics).
    for (int i = 0; i < iters; ++i) {
        int op = opdist(rng);
        int k = keydist(rng);
        if (op < 5) {
            if (refset.insert(k).second) s.insert(k);
        } else if (op < 8) {
            s.erase(k);
            refset.erase(k);
        } else {
            bool in_s = s.find(k) != s.end();
            bool in_r = refset.find(k) != refset.end();
            if (in_s != in_r) {
                printf("  FAIL: key=%d existence mismatch at iter %d\n", k, i);
                return 1;
            }
        }
    }
    if (s.size() != refset.size()) {
        printf("  FAIL: size mismatch s=%zu ref=%zu\n",
               (size_t)s.size(), (size_t)refset.size());
        return 1;
    }
    for (int k : refset) {
        if (s.find(k) == s.end()) {
            printf("  FAIL: key=%d missing in final check\n", k);
            return 1;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  size=%zu : %.1fms  PASS\n", (size_t)s.size(), ms);
    return 0;
}

// =============================================================================
// Main
// =============================================================================
int main() {
    printf("=========================================================\n");
    printf("  find_slot_bucket self-loop-check removal regression\n");
    printf("=========================================================\n");
    int rc = 0;

    // ----- hash_table8.hpp find_slot_bucket (slot_to_bucket/erase_slot) -----
    using M0 = emhash8::HashMap<int, int, AllSameHash>;
    using M1 = emhash8::HashMap<int, int, ZeroLowBitsHash>;
    using M2 = emhash8::HashMap<int, int, TinyRangeHash>;
    using M3 = emhash8::HashMap<int, int, PrimeChainHash>;

    rc |= test_erase_triggers_find_slot<M0, AllSameHash>   ("erase-front SameHash",      100);
    rc |= test_erase_triggers_find_slot<M1, ZeroLowBitsHash>("erase-front ZeroLow",     100);
    rc |= test_erase_triggers_find_slot<M2, TinyRangeHash>  ("erase-front TinyRange",   100);

    rc |= test_erase_iter_const_hash<M0, AllSameHash>      ("erase-iter AllSame",       100);
    rc |= test_erase_iter_const_hash<M2, TinyRangeHash>    ("erase-iter TinyRange",     100);

    rc |= test_random_churn<M0, AllSameHash>               ("churn AllSame",    10000, 1);
    rc |= test_random_churn<M1, ZeroLowBitsHash>           ("churn ZeroLow",    10000, 2);
    rc |= test_random_churn<M2, TinyRangeHash>             ("churn TinyRange",  10000, 3);
    rc |= test_random_churn<M3, PrimeChainHash>            ("churn PrimeChain", 10000, 4);

    rc |= test_concurrent_erase<M0, AllSameHash>           ("concurrent AllSame", 1024, 1);
    rc |= test_concurrent_erase<M2, TinyRangeHash>         ("concurrent TinyRange", 1024, 1);

    rc |= test_erase_last<M0, AllSameHash>                 ("erase-last AllSame",  500);
    rc |= test_erase_last<M2, TinyRangeHash>               ("erase-last TinyRange",500);

    // ----- hash_set8.hpp find_slot_bucket (mirror) -----
    using S0 = emhash8::HashSet<int, AllSameHash>;
    using S1 = emhash8::HashSet<int, ZeroLowBitsHash>;
    using S2 = emhash8::HashSet<int, TinyRangeHash>;

    rc |= test_set_random_churn<S0, AllSameHash>           ("set churn AllSame",    10000, 5);
    rc |= test_set_random_churn<S1, ZeroLowBitsHash>       ("set churn ZeroLow",    10000, 6);
    rc |= test_set_random_churn<S2, TinyRangeHash>         ("set churn TinyRange",  10000, 7);

    printf("\n=========================================================\n");
    printf("  Result: %s\n", rc == 0 ? "ALL PASS" : "FAILURES PRESENT");
    printf("=========================================================\n");
    return rc;
}
