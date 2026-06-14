// Stress test with bad hash function: all keys hash to the same bucket
// This is the worst case for probe coverage - if get_next_bucket doesn't
// visit all groups, find_empty_slot will loop forever.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

// Bad hash: all keys map to the same hash value
struct BadHash {
    size_t operator()(int key) const { return 42; }
};

// Less bad: only a few distinct hash values
struct FewHash {
    size_t operator()(int key) const { return (key % 4) * 1024; }
};

template<typename MapType>
bool stress_test_bad_hash(const char* name, int num_keys, int timeout_ms = 30000)
{
    MapType map;
    auto start = std::chrono::steady_clock::now();

    int inserted = 0;
    for (int i = 0; i < num_keys; i++) {
        map[i] = i * 10;
        inserted++;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            printf("  %-12s TIMEOUT after %dms (inserted %d/%d)\n", name, (int)elapsed, inserted, num_keys);
            return false;
        }
    }

    // Verify all keys are findable
    int found = 0;
    for (int i = 0; i < num_keys; i++) {
        auto it = map.find(i);
        if (it != map.end() && it->second == i * 10)
            found++;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    bool ok = (found == num_keys);
    printf("  %-12s %s inserted=%d found=%d/%d time=%dms\n",
           name, ok ? "OK" : "FAIL", inserted, found, num_keys, (int)elapsed);
    return ok;
}

template<typename MapType>
bool stress_test_few_hash(const char* name, int num_keys, int timeout_ms = 30000)
{
    MapType map;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_keys; i++) {
        map[i] = i * 10;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            printf("  %-12s TIMEOUT after %dms (at key %d/%d)\n", name, (int)elapsed, i, num_keys);
            return false;
        }
    }

    int found = 0;
    for (int i = 0; i < num_keys; i++) {
        auto it = map.find(i);
        if (it != map.end() && it->second == i * 10)
            found++;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    bool ok = (found == num_keys);
    printf("  %-12s %s found=%d/%d time=%dms\n",
           name, ok ? "OK" : "FAIL", found, num_keys, (int)elapsed);
    return ok;
}

// Test with sequential keys (moderate collisions)
template<typename MapType>
bool stress_test_sequential(const char* name, int num_keys, int timeout_ms = 30000)
{
    MapType map;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_keys; i++) {
        map[i] = i;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            printf("  %-12s TIMEOUT after %dms (at key %d/%d)\n", name, (int)elapsed, i, num_keys);
            return false;
        }
    }

    // Verify
    bool ok = true;
    for (int i = 0; i < num_keys; i++) {
        if (map[i] != i) { ok = false; break; }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    printf("  %-12s %s size=%zu time=%dms\n", name, ok ? "OK" : "FAIL", map.size(), (int)elapsed);
    return ok;
}

// Test insert + erase + reinsert cycle
template<typename MapType>
bool test_erase_reinsert(const char* name, int num_keys, int timeout_ms = 30000)
{
    MapType map;
    auto start = std::chrono::steady_clock::now();

    // Insert
    for (int i = 0; i < num_keys; i++) {
        map[i] = i;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            printf("  %-12s TIMEOUT on insert at key %d\n", name, i);
            return false;
        }
    }

    // Erase half
    for (int i = 0; i < num_keys; i += 2) {
        map.erase(i);
    }

    // Reinsert erased keys with different values
    for (int i = 0; i < num_keys; i += 2) {
        map[i] = i * 100;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) {
            printf("  %-12s TIMEOUT on reinsert at key %d\n", name, i);
            return false;
        }
    }

    // Verify
    bool ok = true;
    for (int i = 0; i < num_keys; i++) {
        auto it = map.find(i);
        int expected = (i % 2 == 0) ? i * 100 : i;
        if (it == map.end() || it->second != expected) { ok = false; break; }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    printf("  %-12s %s size=%zu time=%dms\n", name, ok ? "OK" : "FAIL", map.size(), (int)elapsed);
    return ok;
}

int main()
{
    int pass = 0, fail = 0;
    printf("=== Bad Hash Stress Test (all keys -> same bucket) ===\n");
    // Use 500 keys with BadHash - this is the extreme worst case
    if (stress_test_bad_hash<emilib::HashMap<int, int, BadHash>>("emilib2ss", 500)) pass++; else fail++;
    if (stress_test_bad_hash<emilib2::HashMap<int, int, BadHash>>("emilib2o", 500)) pass++; else fail++;
    if (stress_test_bad_hash<emilib3::HashMap<int, int, BadHash>>("emilib2s", 500)) pass++; else fail++;

    printf("\n=== Few Hash Stress Test (keys -> 4 buckets) ===\n");
    if (stress_test_few_hash<emilib::HashMap<int, int, FewHash>>("emilib2ss", 2000)) pass++; else fail++;
    if (stress_test_few_hash<emilib2::HashMap<int, int, FewHash>>("emilib2o", 2000)) pass++; else fail++;
    if (stress_test_few_hash<emilib3::HashMap<int, int, FewHash>>("emilib2s", 2000)) pass++; else fail++;

    printf("\n=== Sequential Key Stress Test ===\n");
    if (stress_test_sequential<emilib::HashMap<int, int>>("emilib2ss", 100000)) pass++; else fail++;
    if (stress_test_sequential<emilib2::HashMap<int, int>>("emilib2o", 100000)) pass++; else fail++;
    if (stress_test_sequential<emilib3::HashMap<int, int>>("emilib2s", 100000)) pass++; else fail++;

    printf("\n=== Erase + Reinsert Cycle Test ===\n");
    if (test_erase_reinsert<emilib::HashMap<int, int, BadHash>>("emilib2ss", 500)) pass++; else fail++;
    if (test_erase_reinsert<emilib2::HashMap<int, int, BadHash>>("emilib2o", 500)) pass++; else fail++;
    if (test_erase_reinsert<emilib3::HashMap<int, int, BadHash>>("emilib2s", 500)) pass++; else fail++;

    printf("\n=== Summary: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
