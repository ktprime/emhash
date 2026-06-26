// High-intensity test for sentinel cleanup: copy, clear, move, rehash
// Verifies no memory errors after removing sentinel placement-new/destructors.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <cassert>
#include <functional>

#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"
#include "emilib/emihset2.hpp"
#include "emilib/emihset3.hpp"
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emhash/hash_set2.hpp"
#include "emhash/hash_set4.hpp"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

// Suppress [[nodiscard]] warnings for emplace/erase/reserve in test code
template<typename T> inline void discard(T&&) {}

static std::string make_key(int i, int len = 16) {
    char buf[64];
    snprintf(buf, sizeof(buf), "key_%0*d", len - 5, i);
    return buf;
}

static std::string make_val(int i) {
    char buf[64];
    snprintf(buf, sizeof(buf), "val_%d_%s", i, "padding_to_avoid_sso");
    return buf;
}

// ============================================================================
// Test template for hash maps
// ============================================================================

template<typename Map>
void test_map_copy_clear_move_rehash(const char* name) {
    printf("  [%s] copy/clear/move/rehash stress...\n", name);

    // 1. Insert many string entries
    {
        Map m;
        const int N = 500;
        for (int i = 0; i < N; i++)
            discard(m.emplace(make_key(i), make_val(i)));
        CHECK(m.size() == (size_t)N, "insert count");

        // 2. Copy constructor
        Map m2(m);
        CHECK(m2.size() == (size_t)N, "copy ctor size");
        CHECK(m2.find(make_key(N/2)) != m2.end(), "copy ctor find");

        // 3. Copy
        Map m3(m);
        CHECK(m3.size() == (size_t)N, "copy assign size");

        // 4. Clear and verify empty
        m.clear();
        CHECK(m.size() == 0, "clear size");
        CHECK(m.empty(), "empty after clear");

        // 5. Reuse after clear (triggers rehash with different bucket count)
        for (int i = 0; i < N/2; i++)
            discard(m.emplace(make_key(i + 1000), make_val(i + 1000)));
        CHECK(m.size() == (size_t)(N/2), "reuse after clear");

        // 6. Move constructor
        Map m4(std::move(m2));
        CHECK(m4.size() == (size_t)N, "move ctor size");
        CHECK(m2.empty() || m2.size() == 0, "moved-from is empty");

        // 7. Move
        Map m5(std::move(m3));
        CHECK(m5.size() == (size_t)N, "move assign size");

        // 8. Reserve (forces rehash)
        discard(m5.reserve(10000));
        CHECK(m5.size() == (size_t)N, "reserve preserves size");

        // 9. Erase half and reinsert (tests rehash with tombstones)
        std::vector<std::string> keys_to_erase;
        int erase_count = 0;
        for (auto& kv : m5) {
            if (erase_count % 2 == 0 && erase_count < N)
                keys_to_erase.push_back(kv.first);
            erase_count++;
        }
        for (auto& k : keys_to_erase)
            discard(m5.erase(k));
        CHECK((int)m5.size() == N - (int)keys_to_erase.size(), "erase half");

        // Reinsert erased entries
        for (int i = 0; i < N; i++) {
            auto key = make_key(i);
            if (m5.find(key) == m5.end()) {
                discard(m5.emplace(key, make_val(i)));
            }
        }
        CHECK(m5.size() == (size_t)N, "reinsert after erase");
    }

    // 10. Repeated copy/clear cycle (high stress)
    {
        Map m;
        const int N = 200;
        for (int cycle = 0; cycle < 20; cycle++) {
            for (int i = 0; i < N; i++)
                discard(m.emplace(make_key(i + cycle * 1000), make_val(i + cycle * 1000)));

            Map mcopy(m);
            CHECK(mcopy.size() == m.size(), "cycle copy");

            m.clear();
            CHECK(m.empty(), "cycle clear");
        }
    }

    // 11. Move from small to large and back
    {
        Map small;
        for (int i = 0; i < 10; i++)
            discard(small.emplace(make_key(i), make_val(i)));

        Map large;
        for (int i = 0; i < 500; i++)
            discard(large.emplace(make_key(i + 100), make_val(i + 100)));

        Map m1(std::move(large));
        large = std::move(small);
        CHECK(large.size() == 10, "move large->small");
        CHECK(m1.size() == 500, "move small->large");
    }

    printf("  [%s] copy/clear/move/rehash stress: done\n", name);
}

// ============================================================================
// Test template for hash sets
// ============================================================================

template<typename Set>
void test_set_copy_clear_move_rehash(const char* name) {
    printf("  [%s] copy/clear/move/rehash stress...\n", name);

    {
        Set s;
        const int N = 500;
        for (int i = 0; i < N; i++)
            discard(s.emplace(make_key(i)));
        CHECK(s.size() == (size_t)N, "insert count");

        Set s2(s);
        CHECK(s2.size() == (size_t)N, "copy ctor size");

        Set s3(s);
        CHECK(s3.size() == (size_t)N, "copy assign size");

        s.clear();
        CHECK(s.size() == 0, "clear size");
        CHECK(s.empty(), "empty after clear");

        for (int i = 0; i < N/2; i++)
            discard(s.emplace(make_key(i + 1000)));
        CHECK(s.size() == (size_t)(N/2), "reuse after clear");

        Set s4(std::move(s2));
        CHECK(s4.size() == (size_t)N, "move ctor size");

        Set s5(std::move(s3));
        CHECK(s5.size() == (size_t)N, "move assign size");

        discard(s5.reserve(10000));
        CHECK(s5.size() == (size_t)N, "reserve preserves size");

        // Erase half and reinsert
        std::vector<std::string> set_keys_to_erase;
        int set_erase_count = 0;
        for (auto& k : s5) {
            if (set_erase_count % 2 == 0 && set_erase_count < N)
                set_keys_to_erase.push_back(k);
            set_erase_count++;
        }
        for (auto& k : set_keys_to_erase)
            discard(s5.erase(k));
        CHECK(s5.size() == (size_t)(N - (int)set_keys_to_erase.size()), "erase half");

        for (int i = 0; i < N; i++) {
            auto key = make_key(i);
            if (s5.find(key) == s5.end())
                discard(s5.emplace(key));
        }
        CHECK(s5.size() == (size_t)N, "reinsert after erase");
    }

    {
        Set s;
        const int N = 200;
        for (int cycle = 0; cycle < 20; cycle++) {
            for (int i = 0; i < N; i++)
                discard(s.emplace(make_key(i + cycle * 1000)));

            Set scopy(s);
            CHECK(scopy.size() == s.size(), "cycle copy");

            s.clear();
            CHECK(s.empty(), "cycle clear");
        }
    }

    {
        Set small, large;
        for (int i = 0; i < 10; i++)
            discard(small.emplace(make_key(i)));
        for (int i = 0; i < 500; i++)
            discard(large.emplace(make_key(i + 100)));

        Set s1(std::move(large));
        large = std::move(small);
        CHECK(large.size() == 10, "move large->small");
        CHECK(s1.size() == 500, "move small->large");
    }

    printf("  [%s] copy/clear/move/rehash stress: done\n", name);
}

// ============================================================================
// Size sweep: test at various bucket counts (boundary conditions)
// ============================================================================

template<typename Map>
void test_map_size_sweep(const char* name) {
    printf("  [%s] size sweep...\n", name);
    const int sizes[] = {0, 1, 2, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 511, 512, 1000, 2000};

    for (int N : sizes) {
        Map m;
        for (int i = 0; i < N; i++)
            discard(m.emplace(make_key(i), make_val(i)));
        CHECK((int)m.size() == N, "size sweep insert");

        Map m2(m);
        CHECK((int)m2.size() == N, "size sweep copy");

        m.clear();
        CHECK(m.empty(), "size sweep clear");

        Map m3(std::move(m2));
        CHECK((int)m3.size() == N, "size sweep move");
    }
    printf("  [%s] size sweep: done\n", name);
}

template<typename Set>
void test_set_size_sweep(const char* name) {
    printf("  [%s] size sweep...\n", name);
    const int sizes[] = {0, 1, 2, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 511, 512, 1000, 2000};

    for (int N : sizes) {
        Set s;
        for (int i = 0; i < N; i++)
            discard(s.emplace(make_key(i)));
        CHECK((int)s.size() == N, "size sweep insert");

        Set s2(s);
        CHECK((int)s2.size() == N, "size sweep copy");

        s.clear();
        CHECK(s.empty(), "size sweep clear");

        Set s3(std::move(s2));
        CHECK((int)s3.size() == N, "size sweep move");
    }
    printf("  [%s] size sweep: done\n", name);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("=== Sentinel Cleanup: High-Intensity Copy/Clear/Move/Rehash Test ===\n\n");

    // emilib maps
    test_map_copy_clear_move_rehash<emilib::HashMap<std::string, std::string>>("emihmap1");
    test_map_copy_clear_move_rehash<emilib2::HashMap<std::string, std::string>>("emihmap2");
    test_map_copy_clear_move_rehash<emilib3::HashMap<std::string, std::string>>("emihmap3");

    // emilib sets
    test_set_copy_clear_move_rehash<emilib2::HashSet<std::string>>("emihset2");
    test_set_copy_clear_move_rehash<emilib3::HashSet<std::string>>("emihset3");

    // emhash maps
    test_map_copy_clear_move_rehash<emhash5::HashMap<std::string, std::string>>("emhash5");
    test_map_copy_clear_move_rehash<emhash6::HashMap<std::string, std::string>>("emhash6");
    test_map_copy_clear_move_rehash<emhash7::HashMap<std::string, std::string>>("emhash7");
    test_map_copy_clear_move_rehash<emhash8::HashMap<std::string, std::string>>("emhash8");

    // emhash sets
    test_set_copy_clear_move_rehash<emhash2::HashSet<std::string>>("emhash_set2");
    test_set_copy_clear_move_rehash<emhash4::HashSet<std::string>>("emhash_set4");

    // Size sweep
    printf("\n=== Size Sweep Tests ===\n");
    test_map_size_sweep<emilib::HashMap<std::string, std::string>>("emihmap1");
    test_map_size_sweep<emilib2::HashMap<std::string, std::string>>("emihmap2");
    test_map_size_sweep<emilib3::HashMap<std::string, std::string>>("emihmap3");
    test_set_size_sweep<emilib2::HashSet<std::string>>("emihset2");
    test_set_size_sweep<emilib3::HashSet<std::string>>("emihset3");
    test_map_size_sweep<emhash5::HashMap<std::string, std::string>>("emhash5");
    test_map_size_sweep<emhash6::HashMap<std::string, std::string>>("emhash6");
    test_map_size_sweep<emhash7::HashMap<std::string, std::string>>("emhash7");
    test_map_size_sweep<emhash8::HashMap<std::string, std::string>>("emhash8");
    test_set_size_sweep<emhash2::HashSet<std::string>>("emhash_set2");
    test_set_size_sweep<emhash4::HashSet<std::string>>("emhash_set4");

    printf("\n=== Summary ===\n");
    printf("  Total:  %d\n", g_pass + g_fail);
    printf("  Passed: %d\n", g_pass);
    printf("  Failed: %d\n", g_fail);

    if (g_fail > 0) {
        printf("\n*** %d TESTS FAILED ***\n", g_fail);
        return 1;
    }
    printf("\n*** ALL %d TESTS PASSED ***\n", g_pass);
    return 0;
}
