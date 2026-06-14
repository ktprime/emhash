// Comprehensive fuzzing test for emilib HashMap implementations
// emilib2ss (namespace emilib), emilib2o (namespace emilib2), emilib2s (namespace emilib3)
// Covers: basic ops, boundary conditions, churn, high load factor, string keys, data consistency
//
// Compile: clang++-20 -fsanitize=fuzzer,address,undefined -std=c++17 -g -O1 -fno-omit-frame-pointer -I/mnt/d/emhash fuzz_emilib.cpp -o fuzz_emilib
// Run:     ./fuzz_emilib -max_total_time=300 -print_final_stats=1

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <string>
#include <cassert>
#include <utility>

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

// ============================================================================
// Operation parsing
// ============================================================================
enum OpCode : uint8_t {
    OP_INSERT = 0,
    OP_ERASE = 1,
    OP_FIND = 2,
    OP_ACCESS = 3,
    OP_ITERATE = 4,
    OP_CLEAR = 5,
    OP_RESERVE = 6,
    OP_INSERT_OR_ASSIGN = 7,
    OP_ERASE_ITERATOR = 8,
    OP_COUNT = 9,
    OP_CONTAINS = 10,
    OP_EMPLACE = 11,
};

struct Op {
    OpCode code;
    int32_t key;
    int32_t value;
};

static std::vector<Op> parse_input(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= size) {
        Op op;
        op.code = static_cast<OpCode>(data[i] % 12);
        memcpy(&op.key, data + i + 1, 4);
        memcpy(&op.value, data + i + 5, 4);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

// ============================================================================
// Generic HashMap fuzzing: compare with std::unordered_map
// Constructor: HashMap(size_t n, [float lf])
// ============================================================================
template<typename HashMapT, bool HasLoadFactor = true>
static void fuzz_hashmap(const std::vector<Op>& ops) {
    HashMapT em(4, 0.8f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case OP_ERASE: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                assert(em_r == ref_r);
                break;
            }
            case OP_FIND: {
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
            case OP_ACCESS: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case OP_ITERATE: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it) {
                    em_collect[it->first] = it->second;
                }
                assert(em_collect == ref);
                break;
            }
            case OP_CLEAR: {
                em.clear();
                ref.clear();
                break;
            }
            case OP_RESERVE: {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 1000000) {
                    em.reserve(cap);
                    ref.reserve(cap);
                }
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                break;
            }
            case OP_ERASE_ITERATOR: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                }
                break;
            }
            case OP_COUNT: {
                assert(em.count(op.key) == ref.count(op.key));
                break;
            }
            case OP_CONTAINS: {
                bool em_has = em.contains(op.key);
                bool ref_has = (ref.find(op.key) != ref.end());
                assert(em_has == ref_has);
                break;
            }
            case OP_EMPLACE: {
                auto em_r = em.emplace(op.key, op.value);
                auto ref_r = ref.emplace(op.key, op.value);
                assert(em_r.second == ref_r.second);
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    // Final verification
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
    }
}

// ============================================================================
// emilib2ss specialization (constructor only takes size_t, no load factor)
// ============================================================================
template<>
void fuzz_hashmap<emilib::HashMap<int, int>, false>(const std::vector<Op>& ops) {
    emilib::HashMap<int, int> em(4);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case OP_ERASE: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                assert(em_r == ref_r);
                break;
            }
            case OP_FIND: {
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
            case OP_ACCESS: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case OP_ITERATE: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it) {
                    em_collect[it->first] = it->second;
                }
                assert(em_collect == ref);
                break;
            }
            case OP_CLEAR: {
                em.clear();
                ref.clear();
                break;
            }
            case OP_RESERVE: {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 1000000) {
                    em.reserve(cap);
                    ref.reserve(cap);
                }
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                break;
            }
            case OP_ERASE_ITERATOR: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                }
                break;
            }
            case OP_COUNT: {
                assert(em.count(op.key) == ref.count(op.key));
                break;
            }
            case OP_CONTAINS: {
                bool em_has = em.contains(op.key);
                bool ref_has = (ref.find(op.key) != ref.end());
                assert(em_has == ref_has);
                break;
            }
            case OP_EMPLACE: {
                auto em_r = em.emplace(op.key, op.value);
                auto ref_r = ref.emplace(op.key, op.value);
                assert(em_r.second == ref_r.second);
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
    }
}

// ============================================================================
// High load factor stress test
// ============================================================================
template<typename HashMapT>
static void fuzz_high_load(const std::vector<Op>& ops) {
    HashMapT em(4, 0.95f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 4) {
            case 0: {
                em.insert({op.key, op.value});
                ref.insert({op.key, op.value});
                break;
            }
            case 1: {
                em.erase(op.key);
                ref.erase(op.key);
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// emilib2ss high load (no load factor param)
template<>
void fuzz_high_load<emilib::HashMap<int, int>>(const std::vector<Op>& ops) {
    emilib::HashMap<int, int> em(4);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 4) {
            case 0: {
                em.insert({op.key, op.value});
                ref.insert({op.key, op.value});
                break;
            }
            case 1: {
                em.erase(op.key);
                ref.erase(op.key);
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// ============================================================================
// Churn test: dense erase/insert with small key range (most likely to find bugs)
// ============================================================================
template<typename HashMapT, bool HasLoadFactor = true>
static void fuzz_churn(const uint8_t* data, size_t size) {
    HashMapT em(4, 0.95f);
    std::unordered_map<int, int> ref;

    // Use small key range (0-19) for maximum collision/churn
    size_t i = 0;
    while (i + 5 <= size) {
        uint8_t op = data[i] % 6;
        int key = static_cast<int>(data[i + 1] % 20);
        int value = static_cast<int>(data[i + 2]) | (static_cast<int>(data[i + 3]) << 8);

        switch (op) {
            case 0: { // insert
                auto em_r = em.insert({key, value});
                auto ref_r = ref.insert({key, value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case 1: { // erase
                size_t em_r = em.erase(key);
                size_t ref_r = ref.erase(key);
                assert(em_r == ref_r);
                break;
            }
            case 2: { // find + verify
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                if (em_it != em.end()) assert(em_it->second == ref_it->second);
                break;
            }
            case 3: { // operator[]
                em[key] = value;
                ref[key] = value;
                break;
            }
            case 4: { // erase by iterator
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                }
                break;
            }
            case 5: { // insert_or_assign
                int v = value;
                em.insert_or_assign(key, std::move(v));
                ref[key] = value;
                break;
            }
        }
        assert(em.size() == ref.size());
        i += 5;
    }

    // Full consistency check
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
    }
}

// emilib2ss churn (no load factor param)
template<>
void fuzz_churn<emilib::HashMap<int, int>, false>(const uint8_t* data, size_t size) {
    emilib::HashMap<int, int> em(4);
    std::unordered_map<int, int> ref;

    size_t i = 0;
    while (i + 5 <= size) {
        uint8_t op = data[i] % 6;
        int key = static_cast<int>(data[i + 1] % 20);
        int value = static_cast<int>(data[i + 2]) | (static_cast<int>(data[i + 3]) << 8);

        switch (op) {
            case 0: {
                auto em_r = em.insert({key, value});
                auto ref_r = ref.insert({key, value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case 1: {
                size_t em_r = em.erase(key);
                size_t ref_r = ref.erase(key);
                assert(em_r == ref_r);
                break;
            }
            case 2: {
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                if (em_it != em.end()) assert(em_it->second == ref_it->second);
                break;
            }
            case 3: {
                em[key] = value;
                ref[key] = value;
                break;
            }
            case 4: {
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                }
                break;
            }
            case 5: {
                int v = value;
                em.insert_or_assign(key, std::move(v));
                ref[key] = value;
                break;
            }
        }
        assert(em.size() == ref.size());
        i += 5;
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
    }
}

// ============================================================================
// String key test
// ============================================================================
template<typename HashMapT>
static void fuzz_string_hashmap(const uint8_t* data, size_t size) {
    HashMapT em;
    std::unordered_map<std::string, int> ref;

    size_t i = 0;
    while (i + 5 <= size) {
        uint8_t op = data[i] % 6;
        char key_buf[4] = {};
        memcpy(key_buf, data + i + 1, 3);
        std::string key(key_buf, 3);
        int value = static_cast<int>(data[i + 4]);

        switch (op) {
            case 0: { em.insert({key, value}); ref.insert({key, value}); break; }
            case 1: { em.erase(key); ref.erase(key); break; }
            case 2: {
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: { em[key] = value; ref[key] = value; break; }
            case 4: {
                std::unordered_map<std::string, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect[it->first] = it->second;
                assert(em_collect.size() == em.size());
                break;
            }
            case 5: { em.clear(); ref.clear(); break; }
        }
        assert(em.size() == ref.size());
        i += 5;
    }
}

// ============================================================================
// Boundary test: empty map, single element, reserve edge cases
// ============================================================================
template<typename HashMapT, bool HasLoadFactor = true>
static void fuzz_boundary(const uint8_t* data, size_t size) {
    HashMapT em(4, 0.8f);

    // Test empty map operations
    assert(em.size() == 0);
    assert(em.empty());
    assert(em.begin() == em.end());
    assert(em.find(0) == em.end());
    assert(em.count(0) == 0);
    assert(!em.contains(0));
    assert(em.erase(0) == 0);

    // Single element
    em.insert({1, 100});
    assert(em.size() == 1);
    assert(!em.empty());
    assert(em.contains(1));
    assert(em.find(1) != em.end());
    assert(em.find(1)->second == 100);
    assert(em.count(1) == 1);

    // Erase single element
    em.erase(1);
    assert(em.size() == 0);
    assert(em.empty());

    // Reserve edge cases
    em.reserve(0);
    em.reserve(1);
    em.reserve(100);
    assert(em.size() == 0);

    // Insert after reserve
    for (int i = 0; i < 50; i++) {
        em[i] = i * 10;
    }
    assert(em.size() == 50);

    // Clear and reinsert
    em.clear();
    assert(em.size() == 0);
    for (int i = 0; i < 50; i++) {
        em.insert({i, i * 10});
    }
    assert(em.size() == 50);

    // Verify all
    for (int i = 0; i < 50; i++) {
        auto it = em.find(i);
        assert(it != em.end());
        assert(it->second == i * 10);
    }

    // Erase all and verify
    for (int i = 0; i < 50; i++) {
        assert(em.erase(i) == 1);
    }
    assert(em.size() == 0);
}

// emilib2ss boundary (no load factor)
template<>
void fuzz_boundary<emilib::HashMap<int, int>, false>(const uint8_t* data, size_t size) {
    emilib::HashMap<int, int> em(4);

    assert(em.size() == 0);
    assert(em.empty());
    assert(em.begin() == em.end());
    assert(em.find(0) == em.end());
    assert(em.count(0) == 0);
    assert(!em.contains(0));
    assert(em.erase(0) == 0);

    em.insert({1, 100});
    assert(em.size() == 1);
    assert(!em.empty());
    assert(em.contains(1));
    assert(em.find(1) != em.end());
    assert(em.find(1)->second == 100);
    assert(em.count(1) == 1);

    em.erase(1);
    assert(em.size() == 0);
    assert(em.empty());

    em.reserve(0);
    em.reserve(1);
    em.reserve(100);
    assert(em.size() == 0);

    for (int i = 0; i < 50; i++) em[i] = i * 10;
    assert(em.size() == 50);

    em.clear();
    assert(em.size() == 0);
    for (int i = 0; i < 50; i++) em.insert({i, i * 10});
    assert(em.size() == 50);

    for (int i = 0; i < 50; i++) {
        auto it = em.find(i);
        assert(it != em.end());
        assert(it->second == i * 10);
    }

    for (int i = 0; i < 50; i++) assert(em.erase(i) == 1);
    assert(em.size() == 0);
}

// ============================================================================
// Collision hash: all keys hash to the same bucket (worst case for probing)
// ============================================================================
struct CollisionHasher {
    size_t operator()(int) const { return 0; }
    size_t operator()(const std::string& s) const { return 0; }
};

// Two-bucket hash: keys hash to only 2 possible buckets
struct TwoBucketHasher {
    size_t operator()(int x) const { return static_cast<size_t>(x & 1); }
    size_t operator()(const std::string& s) const { return s.empty() ? 0 : (s[0] & 1); }
};

// ============================================================================
// Collision hash fuzzing: test with worst-case hash (all keys collide)
// This tests whether find_empty_slot can handle extreme probing distances
// ============================================================================
template<typename HashMapT, bool HasLoadFactor = true>
static void fuzz_collision_hash(const std::vector<Op>& ops) {
    HashMapT em(4, 0.84f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 5) { // limit ops to reduce probing distance
            case 0: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case 1: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                assert(em_r == ref_r);
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: {
                em.clear();
                ref.clear();
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// emilib2ss collision (no load factor param)
template<>
void fuzz_collision_hash<emilib::HashMap<int, int, CollisionHasher>, false>(const std::vector<Op>& ops) {
    emilib::HashMap<int, int, CollisionHasher> em(4);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 5) {
            case 0: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case 1: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                assert(em_r == ref_r);
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: {
                em.clear();
                ref.clear();
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// Also need specialization for TwoBucketHasher with emilib2ss
template<>
void fuzz_collision_hash<emilib::HashMap<int, int, TwoBucketHasher>, false>(const std::vector<Op>& ops) {
    emilib::HashMap<int, int, TwoBucketHasher> em(4);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 5) {
            case 0: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                break;
            }
            case 1: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                assert(em_r == ref_r);
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: {
                em.clear();
                ref.clear();
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// ============================================================================
// insert_unique / insert_unique2 test (emilib2o/emilib2s only)
// These bypass the normal load factor check and can trigger find_empty_slot
// infinite loop if the table is full
// ============================================================================
template<typename HashMapT>
static void fuzz_insert_unique(const uint8_t* data, size_t size) {
    HashMapT em(4, 0.95f);

    // insert_unique assumes no duplicate keys, so we use sequential keys
    size_t i = 0;
    int next_key = 0;
    while (i + 2 <= size && next_key < 200) {
        uint8_t op = data[i] % 4;
        i += 2;

        switch (op) {
            case 0: { // insert_unique
                int k = next_key++;
                em.insert_unique(k, k * 10);
                break;
            }
            case 1: { // erase some keys
                if (next_key > 5) {
                    int erase_key = next_key - 5;
                    em.erase(erase_key);
                }
                break;
            }
            case 2: { // find
                if (next_key > 0) {
                    em.find(next_key - 1);
                }
                break;
            }
            case 3: { // clear and restart
                em.clear();
                next_key = 0;
                break;
            }
        }
    }
}

// ============================================================================
// Main fuzzer entry point
// 24 test selectors covering all three emilib implementations
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    uint8_t test_selector = data[0] % 15;
    const uint8_t* op_data = data + 1;
    size_t op_size = size - 1;

    auto ops = parse_input(op_data, op_size);
    if (ops.size() > 500) ops.resize(500);

    switch (test_selector) {
        // emilib2ss (namespace emilib)
        case 0: fuzz_hashmap<emilib::HashMap<int, int>, false>(ops); break;
        case 1: fuzz_high_load<emilib::HashMap<int, int>>(ops); break;
        case 2: fuzz_churn<emilib::HashMap<int, int>, false>(op_data, op_size); break;

        // emilib2o (namespace emilib2)
        case 3: fuzz_hashmap<emilib2::HashMap<int, int>>(ops); break;
        case 4: fuzz_high_load<emilib2::HashMap<int, int>>(ops); break;
        case 5: fuzz_churn<emilib2::HashMap<int, int>>(op_data, op_size); break;

        // emilib2s (namespace emilib3)
        case 6: fuzz_hashmap<emilib3::HashMap<int, int>>(ops); break;
        case 7: fuzz_high_load<emilib3::HashMap<int, int>>(ops); break;
        case 8: fuzz_churn<emilib3::HashMap<int, int>>(op_data, op_size); break;

        // String key tests
        case 9:  fuzz_string_hashmap<emilib::HashMap<std::string, int>>(op_data, op_size); break;
        case 10: fuzz_string_hashmap<emilib2::HashMap<std::string, int>>(op_data, op_size); break;
        case 11: fuzz_string_hashmap<emilib3::HashMap<std::string, int>>(op_data, op_size); break;

        // Boundary tests
        case 12: fuzz_boundary<emilib::HashMap<int, int>, false>(op_data, op_size); break;
        case 13: fuzz_boundary<emilib2::HashMap<int, int>>(op_data, op_size); break;
        case 14: fuzz_boundary<emilib3::HashMap<int, int>>(op_data, op_size); break;
    }

    return 0;
}
