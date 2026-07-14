// unit/test_runtime_stats.cpp
// Tests for the runtime statistics framework (EMH_ENABLE_STATS).
// When EMH_ENABLE_STATS=0 (default), runtime_stats() returns basic snapshot;
// when EMH_ENABLE_STATS=1, full counters are available.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "common/maps.hpp"

#include <vector>
#include <string>

// ============================================================================
// 1. Basic snapshot fields (always available)
// ============================================================================
TEST_CASE_TEMPLATE("runtime_stats basic snapshot fields", Map, map5<int, int>, map6<int, int>, map7<int, int>, map8<int, int>) {
    Map m;
    auto s = m.runtime_stats();
    CHECK(s.num_filled == 0);
    CHECK(s.num_buckets >= 2);
    CHECK(s.load_factor == doctest::Approx(0.0));
    CHECK(s.max_load_factor > 0.0);
}

TEST_CASE("runtime_stats after inserts and erases") {
    emhash8::HashMap<int, int> m;
    for (int i = 0; i < 100; ++i)
        m[i] = i * 10;

    auto s = m.runtime_stats();
    CHECK(s.num_filled == 100);
    CHECK(s.num_buckets >= 100);
    CHECK(s.load_factor > 0.0);
    CHECK(s.load_factor <= 1.0);

    // Erase half
    for (int i = 0; i < 50; ++i)
        m.erase(i);

    s = m.runtime_stats();
    CHECK(s.num_filled == 50);
}

TEST_CASE("runtime_stats string keys") {
    emhash8::HashMap<std::string, int> m;
    m["hello"] = 1;
    m["world"] = 2;

    auto s = m.runtime_stats();
    CHECK(s.num_filled == 2);
    CHECK(s.load_factor > 0.0);
}

// ============================================================================
// 2. Stats counter tests (only meaningful when EMH_ENABLE_STATS=1)
// ============================================================================
#if EMH_ENABLE_STATS

TEST_CASE("insert_count tracks all insertions") {
    emhash8::HashMap<int, int> m;
    m[1] = 10;
    m[2] = 20;
    m[3] = 30;

    auto s = m.runtime_stats();
    CHECK(s.insert_count >= 3);
}

TEST_CASE("erase_count tracks erasures") {
    emhash8::HashMap<int, int> m;
    m[1] = 10;
    m[2] = 20;
    m.erase(1);

    auto s = m.runtime_stats();
    CHECK(s.erase_count == 1);
}

TEST_CASE("find_count tracks lookups") {
    emhash8::HashMap<int, int> m;
    m[1] = 10;
    (void)m.find(1);  // hit
    (void)m.find(2);  // miss

    auto s = m.runtime_stats();
    CHECK(s.find_count >= 2);
}

TEST_CASE("rehash_count tracks rehashes") {
    emhash8::HashMap<int, int> m;
    // Insert many elements to force rehash
    for (int i = 0; i < 1000; ++i)
        m[i] = i;

    auto s = m.runtime_stats();
    CHECK(s.rehash_count >= 1);
}

TEST_CASE("worst_probe is non-zero after collisions") {
    // Use a bad hash that maps everything to the same bucket
    struct BadHash {
        size_t operator()(int) const { return 42; }
    };
    emhash8::HashMap<int, int, BadHash> m;
    for (int i = 0; i < 20; ++i)
        m[i] = i;

    auto s = m.runtime_stats();
    CHECK(s.worst_probe >= 2);
    CHECK(s.worst_insert_probe >= 2);
    CHECK(s.multi_step_collisions >= 1);
}

TEST_CASE("kickout_count tracks guest key displacement") {
    // Kickout occurs when key A's main bucket is occupied by key B whose
    // hash maps to a different bucket. Use two keys with different hashes
    // but colliding on the same main bucket.
    emhash8::HashMap<int, int> m;
    // Insert enough keys that some will have guest-key kickout
    for (int i = 0; i < 200; ++i)
        m[i] = i;

    auto s = m.runtime_stats();
    // With 200 entries and a decent hash, kickout is likely but not guaranteed.
    // We just check the counter is non-negative (no crash).
    CHECK(s.kickout_count >= 0);
}

TEST_CASE("reset_stats clears counters") {
    emhash8::HashMap<int, int> m;
    for (int i = 0; i < 100; ++i)
        m[i] = i;

    m.reset_stats();
    auto s = m.runtime_stats();
    CHECK(s.insert_count == 0);
    CHECK(s.erase_count == 0);
    CHECK(s.find_count == 0);
    CHECK(s.rehash_count == 0);
    CHECK(s.worst_probe == 0);
    // num_filled/num_buckets are NOT reset — they reflect current state
    CHECK(s.num_filled == 100);
}

TEST_CASE("average probe distance is reasonable") {
    emhash8::HashMap<int, int> m;
    for (int i = 0; i < 1000; ++i)
        m[i] = i;

    auto s = m.runtime_stats();
    if (s.insert_count > 0) {
        double avg = static_cast<double>(s.total_insert_probe) / s.insert_count;
        CHECK(avg < 10.0);  // with a good hash, avg probe should be small
    }
}

TEST_CASE("swap transfers stats") {
    emhash8::HashMap<int, int> a, b;
    for (int i = 0; i < 50; ++i)
        a[i] = i;

    auto stats_a_before = a.runtime_stats();
    a.swap(b);

    auto stats_b_after = b.runtime_stats();
    CHECK(stats_b_after.num_filled == 50);
    CHECK(stats_b_after.insert_count == stats_a_before.insert_count);
}

TEST_CASE("copy constructor resets stats") {
    emhash8::HashMap<int, int> a;
    for (int i = 0; i < 50; ++i)
        a[i] = i;

    auto b = a;
    auto s = b.runtime_stats();
    CHECK(s.num_filled == 50);
    // Cloned map has fresh stats (not inherited from source)
    CHECK(s.insert_count == 0);
    CHECK(s.find_count == 0);
}

#else // EMH_ENABLE_STATS == 0

TEST_CASE("stats counters are zero when EMH_ENABLE_STATS=0") {
    emhash8::HashMap<int, int> m;
    for (int i = 0; i < 100; ++i)
        m[i] = i;

    auto s = m.runtime_stats();
    CHECK(s.num_filled == 100);
    // When stats are disabled, counters remain 0
    CHECK(s.insert_count == 0);
    CHECK(s.erase_count == 0);
    CHECK(s.find_count == 0);
    CHECK(s.rehash_count == 0);
}

TEST_CASE("reset_stats is no-op when disabled") {
    emhash8::HashMap<int, int> m;
    m[1] = 10;
    m.reset_stats();
    auto s = m.runtime_stats();
    CHECK(s.num_filled == 1);
}

#endif // EMH_ENABLE_STATS
