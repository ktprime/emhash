// fuzz/fuzz_extreme.cpp
// Extreme-scenario libfuzzer target: 100% collision + high load factor +
// tiny reserve(1) + rapid churn. Exercises the harshest paths.
//
// Build (libfuzzer): clang++ -fsanitize=fuzzer,address -std=c++17 \
//                         -I../include -Icommon fuzz_extreme.cpp
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>
#include <unordered_map>

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

// ============================================================================
// Extreme scenario 1: reserve(1) + random inserts (tiny-table kickout)
// ============================================================================
template <typename MapT>
static void fuzz_reserve1(const uint8_t* data, size_t size) {
    MapT m;
    m.reserve(1);
    std::unordered_map<int, int> ref;

    size_t i = 0;
    int ops = 0;
    while (i + 4 <= size && ops < 30) {
        int32_t key;
        memcpy(&key, data + i, 4);
        i += 4;
        m[key] = ops;
        ref[key] = ops;
        ++ops;
    }
    assert(m.size() == ref.size());
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        assert(it != m.end());
        assert(it->second == v);
    }
}

// ============================================================================
// Extreme scenario 2: high load factor + collision-heavy keys
// ============================================================================
template <typename MapT>
static void fuzz_high_lf(const uint8_t* data, size_t size) {
    MapT m;
    m.max_load_factor(0.99f);
    m.reserve(64);
    std::unordered_map<int, int> ref;

    // Insert keys that are multiples of 64 (collision under power-of-2 mask)
    size_t i = 0;
    int ops = 0;
    while (i + 4 <= size && ops < 200) {
        int32_t raw;
        memcpy(&raw, data + i, 4);
        i += 4;
        int key = raw * 64;  // force collision
        m[key] = ops;
        ref[key] = ops;
        ++ops;
    }
    assert(m.size() == ref.size());
    for (auto& [k, v] : ref) {
        auto it = m.find(k);
        assert(it != m.end());
        assert(it->second == v);
    }
}

// ============================================================================
// Extreme scenario 3: rapid churn (insert + erase cycle)
// ============================================================================
template <typename MapT>
static void fuzz_churn(const uint8_t* data, size_t size) {
    MapT m;
    std::unordered_map<int, int> ref;

    size_t i = 0;
    int ops = 0;
    while (i + 5 <= size && ops < 500) {
        uint8_t action = data[i] % 3;
        int32_t key;
        memcpy(&key, data + i + 1, 4);
        i += 5;

        if (action == 0) {          // insert
            m[key] = ops;
            ref[key] = ops;
        } else if (action == 1) {   // erase
            m.erase(key);
            ref.erase(key);
        } else {                    // find
            auto eit = m.find(key);
            auto rit = ref.find(key);
            assert((eit == m.end()) == (rit == ref.end()));
        }
        ++ops;
    }
    assert(m.size() == ref.size());
}

// ============================================================================
// libfuzzer entry: cycle through extreme scenarios + all implementations
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 9) return 0;

    uint8_t scenario = data[0] % 3;
    uint8_t impl = data[1] % 7;
    const uint8_t* payload = data + 2;
    size_t payload_size = size - 2;

    // Dispatch: scenario × implementation (selector = scenario*7 + impl, 0..20)
    int selector = scenario * 7 + impl;
    switch (selector) {
        // Scenario 0: reserve(1) — impls 0..6
        case 0:  fuzz_reserve1<emhash5::HashMap<int, int>>(payload, payload_size); break;
        case 1:  fuzz_reserve1<emhash6::HashMap<int, int>>(payload, payload_size); break;
        case 2:  fuzz_reserve1<emhash7::HashMap<int, int>>(payload, payload_size); break;
        case 3:  fuzz_reserve1<emhash8::HashMap<int, int>>(payload, payload_size); break;
        case 4:  fuzz_reserve1<emilib::HashMap<int, int>>(payload, payload_size); break;
        case 5:  fuzz_reserve1<emilib2::HashMap<int, int>>(payload, payload_size); break;
        case 6:  fuzz_reserve1<emilib3::HashMap<int, int>>(payload, payload_size); break;
        // Scenario 1: high LF — impls 0..6
        case 7:  fuzz_high_lf<emhash5::HashMap<int, int>>(payload, payload_size); break;
        case 8:  fuzz_high_lf<emhash6::HashMap<int, int>>(payload, payload_size); break;
        case 9:  fuzz_high_lf<emhash7::HashMap<int, int>>(payload, payload_size); break;
        case 10: fuzz_high_lf<emhash8::HashMap<int, int>>(payload, payload_size); break;
        case 11: fuzz_high_lf<emilib::HashMap<int, int>>(payload, payload_size); break;
        case 12: fuzz_high_lf<emilib2::HashMap<int, int>>(payload, payload_size); break;
        case 13: fuzz_high_lf<emilib3::HashMap<int, int>>(payload, payload_size); break;
        // Scenario 2: churn — impls 0..6
        case 14: fuzz_churn<emhash5::HashMap<int, int>>(payload, payload_size); break;
        case 15: fuzz_churn<emhash6::HashMap<int, int>>(payload, payload_size); break;
        case 16: fuzz_churn<emhash7::HashMap<int, int>>(payload, payload_size); break;
        case 17: fuzz_churn<emhash8::HashMap<int, int>>(payload, payload_size); break;
        case 18: fuzz_churn<emilib::HashMap<int, int>>(payload, payload_size); break;
        case 19: fuzz_churn<emilib2::HashMap<int, int>>(payload, payload_size); break;
        case 20: fuzz_churn<emilib3::HashMap<int, int>>(payload, payload_size); break;
        default: break;
    }
    return 0;
}
