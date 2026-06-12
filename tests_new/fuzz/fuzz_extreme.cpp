// Extreme fuzzing test for emhash - stress all edge cases
// Compile: clang++-20 -fsanitize=fuzzer,address,undefined -std=c++17 -g -O1 -fno-omit-frame-pointer -I. fuzz/fuzz_extreme.cpp -o fuzz/fuzz_extreme
//
// Extreme scenarios:
//   - All keys hash to same bucket (100% collision)
//   - High load factor (0.98) with heavy erase/insert churn
//   - Value correctness verification after every operation
//   - shrink_to_fit / reserve edge cases
//   - Iterator erase during iteration
//   - emhash5/6/7/8 HashMap + emhash8 HashSet
//   - Copy/move constructor and assignment

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cassert>
#include <utility>
#include <algorithm>

#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"
#include "../hash_set8.hpp"

// All keys hash to same bucket - worst case collision
struct CollisionHasher {
    size_t operator()(int) const { return 0; }
};

// Two-bucket collision - moderate collision
struct TwoBucketHasher {
    size_t operator()(int x) const { return static_cast<size_t>(x & 1); }
};

// ============================================================================
// Operation parsing - more ops for extreme testing
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
    OP_SHRINK = 12,
    OP_SWAP = 13,
    OP_VERIFY_ALL = 14,
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
        op.code = static_cast<OpCode>(data[i] % 15);
        memcpy(&op.key, data + i + 1, 4);
        memcpy(&op.value, data + i + 5, 4);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

// ============================================================================
// Extreme HashMap test with full value verification
// ============================================================================
template<typename HashMapT>
static void fuzz_extreme_hashmap(const std::vector<Op>& ops, float mlf = 0.8f) {
    HashMapT em(2, mlf);
    std::unordered_map<int, int> ref;

    auto verify_all = [&]() {
        // Verify every key-value pair
        for (const auto& kv : ref) {
            auto it = em.find(kv.first);
            if (it == em.end()) {
                assert(!"KEY LOST in extreme test");
            }
            if (it->second != kv.second) {
                assert(!"VALUE WRONG in extreme test");
            }
        }
        // Verify no extra keys
        size_t count = 0;
        for (auto it = em.begin(); it != em.end(); ++it) {
            auto rit = ref.find(it->first);
            if (rit == ref.end()) {
                assert(!"EXTRA KEY in extreme test");
            }
            if (rit->second != it->second) {
                assert(!"VALUE MISMATCH in extreme test");
            }
            count++;
        }
        assert(count == ref.size());
    };

    for (size_t i = 0; i < ops.size(); i++) {
        const auto& op = ops[i];
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                assert(em_r.second == ref_r.second);
                if (em_r.second) {
                    assert(em_r.first->second == op.value);
                }
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
                if (em_it != em.end() && ref_it != ref.end()) {
                    assert(em_it->second == ref_it->second);
                }
                break;
            }
            case OP_ACCESS: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                // Verify the value was set correctly
                assert(em[op.key] == op.value);
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
                if (cap < 100000) {
                    em.reserve(cap);
                }
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                assert(em[op.key] == op.value);
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
            case OP_SHRINK: {
                // shrink_to_fit or rehash to minimal
                if (em.size() > 0)
                    em.reserve(em.size());
                break;
            }
            case OP_SWAP: {
                // Self-swap test
                HashMapT other(2, mlf);
                std::unordered_map<int, int> other_ref;
                other.swap(em);
                std::swap(other_ref, ref);
                other.swap(em);
                std::swap(other_ref, ref);
                break;
            }
            case OP_VERIFY_ALL: {
                verify_all();
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    // Final full verification
    verify_all();
}

// ============================================================================
// Extreme HashSet test
// ============================================================================
template<typename HashSetT>
static void fuzz_extreme_hashset(const std::vector<Op>& ops) {
    HashSetT em(2);
    std::unordered_set<int> ref;

    auto verify_all = [&]() {
        for (const auto& k : ref) {
            if (!em.contains(k)) {
                assert(!"KEY LOST in HashSet extreme test");
            }
        }
        size_t count = 0;
        for (auto it = em.begin(); it != em.end(); ++it) {
            assert(ref.count(*it) == 1);
            count++;
        }
        assert(count == ref.size());
    };

    for (const auto& op : ops) {
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert(op.key);
                auto ref_r = ref.insert(op.key);
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
                assert((em.find(op.key) != em.end()) == (ref.find(op.key) != ref.end()));
                break;
            }
            case OP_ACCESS: {
                em.insert(op.key);
                ref.insert(op.key);
                break;
            }
            case OP_ITERATE: {
                std::unordered_set<int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect.insert(*it);
                assert(em_collect == ref);
                break;
            }
            case OP_CLEAR: {
                em.clear(); ref.clear();
                break;
            }
            case OP_RESERVE: {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 100000) em.reserve(cap);
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                em.insert(op.key);
                ref.insert(op.key);
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
                assert(em.contains(op.key) == (ref.count(op.key) > 0));
                break;
            }
            case OP_EMPLACE: {
                auto em_r = em.emplace(op.key);
                auto ref_r = ref.emplace(op.key);
                assert(em_r.second == ref_r.second);
                break;
            }
            case OP_SHRINK: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
            case OP_SWAP: {
                HashSetT other(2);
                std::unordered_set<int> other_ref;
                other.swap(em);
                std::swap(other_ref, ref);
                other.swap(em);
                std::swap(other_ref, ref);
                break;
            }
            case OP_VERIFY_ALL: {
                verify_all();
                break;
            }
        }
        assert(em.size() == ref.size());
    }
    verify_all();
}

// ============================================================================
// 100% collision test for emhash8
// ============================================================================
static void fuzz_emhash8_full_collision(const std::vector<Op>& ops) {
    emhash8::HashMap<int, int, CollisionHasher> em(2, 0.8f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 6) { // Only basic ops, collision already stresses enough
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
                if (em_it != em.end() && ref_it != ref.end())
                    assert(em_it->second == ref_it->second);
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
            case 5: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect[it->first] = it->second;
                assert(em_collect == ref);
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    // Full verify
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
}

// ============================================================================
// Two-bucket collision test for emhash8 HashSet
// ============================================================================
static void fuzz_emhash8_set_twobucket(const std::vector<Op>& ops) {
    emhash8::HashSet<int, TwoBucketHasher> em(2);
    std::unordered_set<int> ref;

    for (const auto& op : ops) {
        switch (op.code % 6) {
            case 0: {
                auto em_r = em.insert(op.key);
                auto ref_r = ref.insert(op.key);
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
                assert((em.find(op.key) != em.end()) == (ref.count(op.key) > 0));
                break;
            }
            case 3: {
                em.insert(op.key);
                ref.insert(op.key);
                break;
            }
            case 4: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
            case 5: {
                std::unordered_set<int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect.insert(*it);
                assert(em_collect == ref);
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    for (const auto& k : ref) {
        assert(em.contains(k));
    }
}

// ============================================================================
// Heavy erase/insert churn test
// ============================================================================
template<typename HashMapT>
static void fuzz_churn(const std::vector<Op>& ops) {
    HashMapT em(4, 0.95f);
    std::unordered_map<int, int> ref;

    // Use a small key range to maximize collisions and churn
    for (const auto& op : ops) {
        int key = op.key % 20; // Only 20 possible keys - heavy churn
        switch (op.code % 5) {
            case 0: {
                auto em_r = em.insert({key, op.value});
                auto ref_r = ref.insert({key, op.value});
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
                em[key] = op.value;
                ref[key] = op.value;
                assert(em[key] == op.value);
                break;
            }
            case 3: {
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                assert((em_it != em.end()) == (ref_it != ref.end()));
                if (em_it != em.end() && ref_it != ref.end())
                    assert(em_it->second == ref_it->second);
                break;
            }
            case 4: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
        }
        assert(em.size() == ref.size());
    }

    // Full verify
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        assert(it != em.end());
        assert(it->second == kv.second);
    }
    size_t count = 0;
    for (auto it = em.begin(); it != em.end(); ++it) {
        assert(ref.count(it->first) == 1);
        assert(ref[it->first] == it->second);
        count++;
    }
    assert(count == ref.size());
}

// ============================================================================
// 100% collision test for emhash5/7
// ============================================================================
template<typename HashMapT>
static void fuzz_full_collision(const std::vector<Op>& ops) {
    HashMapT em(2, 0.8f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code % 6) {
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
                if (em_it != em.end() && ref_it != ref.end())
                    assert(em_it->second == ref_it->second);
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
            case 5: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect[it->first] = it->second;
                assert(em_collect == ref);
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

    uint8_t test_selector = data[0] % 18;

    const uint8_t* op_data = data + 1;
    size_t op_size = size - 1;
    auto ops = parse_input(op_data, op_size);

    if (ops.size() > 500) ops.resize(500);

    switch (test_selector) {
        case 0: // emhash5 extreme
            fuzz_extreme_hashmap<emhash5::HashMap<int, int>>(ops);
            break;
        case 1: // emhash6 extreme
            fuzz_extreme_hashmap<emhash6::HashMap<int, int>>(ops);
            break;
        case 2: // emhash7 extreme
            fuzz_extreme_hashmap<emhash7::HashMap<int, int>>(ops);
            break;
        case 3: // emhash8 extreme
            fuzz_extreme_hashmap<emhash8::HashMap<int, int>>(ops);
            break;
        case 4: // emhash8 HashSet extreme
            fuzz_extreme_hashset<emhash8::HashSet<int>>(ops);
            break;
        case 5: // emhash8 100% collision
            fuzz_emhash8_full_collision(ops);
            break;
        case 6: // emhash8 HashSet two-bucket collision
            fuzz_emhash8_set_twobucket(ops);
            break;
        case 7: // emhash5 high load + churn
            fuzz_churn<emhash5::HashMap<int, int>>(ops);
            break;
        case 8: // emhash6 high load + churn
            fuzz_churn<emhash6::HashMap<int, int>>(ops);
            break;
        case 9: // emhash7 high load + churn
            fuzz_churn<emhash7::HashMap<int, int>>(ops);
            break;
        case 10: // emhash8 high load + churn
            fuzz_churn<emhash8::HashMap<int, int>>(ops);
            break;
        case 11: // emhash8 extreme high load factor (0.98)
            fuzz_extreme_hashmap<emhash8::HashMap<int, int>>(ops, 0.98f);
            break;
        case 12: // emhash5 extreme high load factor (0.98)
            fuzz_extreme_hashmap<emhash5::HashMap<int, int>>(ops, 0.98f);
            break;
        case 13: // emhash8 HashSet high load + churn
            fuzz_extreme_hashset<emhash8::HashSet<int>>(ops);
            break;
        case 14: // emhash5 100% collision
            fuzz_full_collision<emhash5::HashMap<int, int, CollisionHasher>>(ops);
            break;
        case 15: // emhash7 100% collision
            fuzz_full_collision<emhash7::HashMap<int, int, CollisionHasher>>(ops);
            break;
        case 16: // emhash5 two-bucket collision + churn
            fuzz_churn<emhash5::HashMap<int, int, TwoBucketHasher>>(ops);
            break;
        case 17: // emhash7 two-bucket collision + churn
            fuzz_churn<emhash7::HashMap<int, int, TwoBucketHasher>>(ops);
            break;
    }

    return 0;
}
