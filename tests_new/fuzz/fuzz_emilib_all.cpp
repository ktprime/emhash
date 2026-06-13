// Comprehensive fuzzing test for all 3 emilib implementations
// Compile with: clang++ -fsanitize=fuzzer,address -std=c++17 -g fuzz_emilib_all.cpp -o fuzz_emilib_all
//
// Run:
//   ./fuzz_emilib_all -max_total_time=60 -print_final_stats=1
//   ./fuzz_emilib_all corpus/ -max_total_time=300
//
// This fuzzer tests:
//   - emilib::HashMap (emilib2ss)
//   - emilib2::HashMap (emilib2o)
//   - emilib3::HashMap (emilib2s)
//   - Custom hash collision patterns
//   - High load factor scenarios
//   - String key types
//   - All major API operations

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <string>
#include <cassert>
#include <utility>

#include "../../thirdparty/emilib/emilib2ss.hpp"
#include "../../thirdparty/emilib/emilib2o.hpp"
#include "../../thirdparty/emilib/emilib2s.hpp"

// ============================================================================
// Custom hasher for controlled collision patterns
// ============================================================================
struct FuzzHasher {
    uint64_t seed = 0;
    FuzzHasher() = default;
    explicit FuzzHasher(uint64_t s) : seed(s) {}

    size_t operator()(int x) const {
        uint64_t h = static_cast<uint64_t>(x) ^ seed;
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ull;
        h ^= h >> 33;
        return static_cast<size_t>(h);
    }

    size_t operator()(const std::string& s) const {
        uint64_t h = seed;
        for (size_t i = 0; i < s.size(); i++) {
            h = h * 131 + static_cast<uint64_t>(static_cast<unsigned char>(s[i]));
        }
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdull;
        h ^= h >> 33;
        return static_cast<size_t>(h);
    }
};

// ============================================================================
// Collision hasher - all keys hash to same bucket
// ============================================================================
struct CollisionHasher {
    size_t operator()(int x) const {
        // All keys hash to 0 - extreme collision test
        return 0;
    }

    size_t operator()(const std::string& s) const {
        return 0;
    }
};

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
// Helper: run operations on a generic HashMap and compare with reference
// ============================================================================
template<typename HashMapT>
static void fuzz_hashmap(const std::vector<Op>& ops) {
    HashMapT em;
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
// Helper: fuzz string key types
// ============================================================================
template<typename HashMapT>
static void fuzz_string_hashmap(const uint8_t* data, size_t size) {
    HashMapT em;
    std::unordered_map<std::string, int> ref;

    // Use raw bytes as string keys
    size_t i = 0;
    while (i + 5 <= size) {
        uint8_t op = data[i] % 6;
        // Create short string key from next 3 bytes
        char key_buf[4] = {};
        memcpy(key_buf, data + i + 1, 3);
        std::string key(key_buf, 3);
        int value = static_cast<int>(data[i + 4]);

        switch (op) {
            case 0: { // insert
                em.insert({key, value});
                ref.insert({key, value});
                break;
            }
            case 1: { // erase
                em.erase(key);
                ref.erase(key);
                break;
            }
            case 2: { // find
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: { // operator[]
                em[key] = value;
                ref[key] = value;
                break;
            }
            case 4: { // iterate
                std::unordered_map<std::string, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it) {
                    em_collect[it->first] = it->second;
                }
                assert(em_collect.size() == em.size());
                break;
            }
            case 5: { // clear
                em.clear();
                ref.clear();
                break;
            }
        }
        assert(em.size() == ref.size());
        i += 5;
    }
}

// ============================================================================
// Helper: high load factor stress test
// ============================================================================
template<typename HashMapT>
static void fuzz_high_load(const std::vector<Op>& ops) {
    HashMapT em;
    std::unordered_map<int, int> ref;

    // Only insert and find, to stress the high load path
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

    // Verify all elements
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// ============================================================================
// Helper: collision pattern test - all keys hash to same bucket
// ============================================================================
template<typename HashMapT>
static void fuzz_collision_pattern(const std::vector<Op>& ops) {
    HashMapT em;
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 4) {
            case 0: { // insert
                em.insert({op.key, op.value});
                ref.insert({op.key, op.value});
                break;
            }
            case 1: { // erase
                em.erase(op.key);
                ref.erase(op.key);
                break;
            }
            case 2: { // find
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                break;
            }
            case 3: { // operator[]
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    // Verify all elements
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// ============================================================================
// Helper: fuzz with custom hash
// ============================================================================
template<typename HashMapT>
static void fuzz_custom_hash(const std::vector<Op>& ops, uint64_t seed) {
    HashMapT em;
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
                assert((em_it != em.end()) == (ref_it != ref.end()));
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
                assert(em.contains(op.key) == (ref.find(op.key) != ref.end()));
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
}

// ============================================================================
// Main fuzzer entry point
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    // First byte selects which test to run
    uint8_t test_selector = data[0] % 15;

    const uint8_t* op_data = data + 1;
    size_t op_size = size - 1;
    auto ops = parse_input(op_data, op_size);

    // Limit operations to prevent timeout
    if (ops.size() > 500) ops.resize(500);

    switch (test_selector) {
        // emilib::HashMap (emilib2ss) tests
        case 0: // Basic operations
            fuzz_hashmap<emilib::HashMap<int, int>>(ops);
            break;
        case 1: // String key test
            fuzz_string_hashmap<emilib::HashMap<std::string, int>>(op_data, op_size);
            break;
        case 2: // High load factor test
            fuzz_high_load<emilib::HashMap<int, int>>(ops);
            break;
        case 3: // Custom hash test
            fuzz_custom_hash<emilib::HashMap<int, int, FuzzHasher>>(ops, 0xDEADBEEF);
            break;
        case 4: // Collision pattern test
            fuzz_collision_pattern<emilib::HashMap<int, int, CollisionHasher>>(ops);
            break;

        // emilib2::HashMap (emilib2o) tests
        case 5: // Basic operations
            fuzz_hashmap<emilib2::HashMap<int, int>>(ops);
            break;
        case 6: // String key test
            fuzz_string_hashmap<emilib2::HashMap<std::string, int>>(op_data, op_size);
            break;
        case 7: // High load factor test
            fuzz_high_load<emilib2::HashMap<int, int>>(ops);
            break;
        case 8: // Custom hash test
            fuzz_custom_hash<emilib2::HashMap<int, int, FuzzHasher>>(ops, 0xDEADBEEF);
            break;
        case 9: // Collision pattern test
            fuzz_collision_pattern<emilib2::HashMap<int, int, CollisionHasher>>(ops);
            break;

        // emilib3::HashMap (emilib2s) tests
        case 10: // Basic operations
            fuzz_hashmap<emilib3::HashMap<int, int>>(ops);
            break;
        case 11: // String key test
            fuzz_string_hashmap<emilib3::HashMap<std::string, int>>(op_data, op_size);
            break;
        case 12: // High load factor test
            fuzz_high_load<emilib3::HashMap<int, int>>(ops);
            break;
        case 13: // Custom hash test
            fuzz_custom_hash<emilib3::HashMap<int, int, FuzzHasher>>(ops, 0xDEADBEEF);
            break;
        case 14: // Collision pattern test
            fuzz_collision_pattern<emilib3::HashMap<int, int, CollisionHasher>>(ops);
            break;
    }

    return 0;
}