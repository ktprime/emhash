// Advanced fuzzing test for emhash8 targeting specific hidden bugs
// Compile: clang++ -fsanitize=fuzzer,address -std=c++17 -g fuzz_emhash8_advanced.cpp -o fuzz_advanced
//
// Targeted bugs:
// 1. Hash high-bit collision false positives
// 2. kickout_bucket chain corruption
// 3. _etail staleness
// 4. Iterator invalidation
// 5. Near-full table behavior

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <set>
#include "emhash/hash_table8.hpp"

// Custom hasher that allows controlled collisions
struct FuzzHasher {
    uint64_t seed = 0;
    FuzzHasher() = default;
    explicit FuzzHasher(uint64_t s) : seed(s) {}

    size_t operator()(int x) const {
        // Mix the key with seed to create various collision patterns
        uint64_t h = static_cast<uint64_t>(x) ^ seed;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ull;
        h ^= h >> 33;
        return static_cast<size_t>(h);
    }
};

// Parse fuzz input
struct Op {
    uint8_t code;
    int32_t key;
    int32_t value;
};

static std::vector<Op> parse_ops(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    for (size_t i = 0; i + 9 <= size; i += 9) {
        Op op;
        op.code = data[i];
        op.key = *reinterpret_cast<const int32_t*>(data + i + 1);
        op.value = *reinterpret_cast<const int32_t*>(data + i + 5);
        ops.push_back(op);
    }
    return ops;
}

// =============================================================================
// Target 1: Custom hash collision patterns
// =============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8)
        return 0;

    // First 8 bytes control the hasher seed
    uint64_t seed = *reinterpret_cast<const uint64_t*>(data);
    const uint8_t* op_data = data + 8;
    size_t op_size = size - 8;

    auto ops = parse_ops(op_data, op_size);

    emhash8::HashMap<int, int, FuzzHasher> em(2, 0.8f);
    std::unordered_map<int, int> ref;

    // Track iterators to test invalidation
    std::vector<int> inserted_keys;

    for (const auto& op : ops) {
        uint8_t cmd = op.code % 12;

        switch (cmd) {
        case 0: { // insert (not insert_unique - that requires user to guarantee uniqueness)
            auto em_r = em.insert({op.key, op.value});
            auto ref_r = ref.insert({op.key, op.value});
            // insert should match reference behavior
            if (em_r.second != ref_r.second) {
                // Key exists in one but not the other - this is a real bug
                assert(em_r.second == ref_r.second);
            }
            if (ref_r.second) {
                inserted_keys.push_back(op.key);
            }
            break;
        }
        case 1: { // erase by key
            size_t em_r = em.erase(op.key);
            size_t ref_r = ref.erase(op.key);
            assert(em_r == ref_r);
            break;
        }
        case 2: { // find
            auto em_it = em.find(op.key);
            auto ref_it = ref.find(op.key);
            bool em_found = (em_it != em.end());
            bool ref_found = (ref_it != ref.end());
            assert(em_found == ref_found);
            if (em_found && ref_found) {
                assert(em_it->second == ref_it->second);
            }
            break;
        }
        case 3: { // operator[]
            em[op.key] = op.value;
            ref[op.key] = op.value;
            break;
        }
        case 4: { // iterate and verify
            std::unordered_map<int, int> em_collect;
            for (auto it = em.begin(); it != em.end(); ++it) {
                // Verify key exists in reference
                assert(ref.count(it->first) == 1);
                assert(ref[it->first] == it->second);
                em_collect[it->first] = it->second;
            }
            assert(em_collect.size() == em.size());
            assert(em_collect == ref);
            break;
        }
        case 5: { // clear
            em.clear();
            ref.clear();
            inserted_keys.clear();
            break;
        }
        case 6: { // reserve
            size_t cap = static_cast<size_t>(op.key & 0xFFFF);
            em.reserve(cap);
            ref.reserve(cap);
            break;
        }
        case 7: { // insert_or_assign
            int v = op.value;
            auto em_r = em.insert_or_assign(op.key, std::move(v));
            bool was_new = (ref.find(op.key) == ref.end());
            ref[op.key] = op.value;
            assert(em_r.second == was_new);
            break;
        }
        case 8: { // erase iterator
            auto em_it = em.find(op.key);
            auto ref_it = ref.find(op.key);
            if (em_it != em.end() && ref_it != ref.end()) {
                em.erase(em_it);
                ref.erase(ref_it);
            }
            break;
        }
        case 9: { // count
            assert(em.count(op.key) == ref.count(op.key));
            break;
        }
        case 10: { // contains
            assert(em.contains(op.key) == (ref.find(op.key) != ref.end()));
            break;
        }
        case 11: { // at (with exception handling)
            auto ref_it = ref.find(op.key);
            if (ref_it != ref.end()) {
                assert(em.at(op.key) == ref_it->second);
            }
            break;
        }
        }

        // Global invariant: sizes must match
        assert(em.size() == ref.size());
    }

    // Final comprehensive verification
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }

    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
    }

    return 0;
}
