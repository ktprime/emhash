// Extreme fuzzing test for emhash5/6/7/8 - NO hash collision
// Focus: data consistency, edge cases, not just crash
// Compile: clang++-20 -fsanitize=fuzzer,address,undefined -std=c++17 -g -O1 -fno-omit-frame-pointer -I. fuzz/fuzz_nocoll.cpp -o fuzz/fuzz_nocoll

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
        for (const auto& kv : ref) {
            auto it = em.find(kv.first);
            if (it == em.end()) {
                __builtin_trap(); // KEY LOST
            }
            if (it->second != kv.second) {
                __builtin_trap(); // VALUE WRONG
            }
        }
        size_t count = 0;
        for (auto it = em.begin(); it != em.end(); ++it) {
            auto rit = ref.find(it->first);
            if (rit == ref.end()) {
                __builtin_trap(); // EXTRA KEY
            }
            if (rit->second != it->second) {
                __builtin_trap(); // VALUE MISMATCH
            }
            count++;
        }
        if (count != ref.size()) {
            __builtin_trap(); // SIZE MISMATCH
        }
    };

    for (size_t i = 0; i < ops.size(); i++) {
        const auto& op = ops[i];
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                if (em_r.second != ref_r.second) __builtin_trap();
                if (em_r.second && em_r.first->second != op.value) __builtin_trap();
                break;
            }
            case OP_ERASE: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                if (em_r != ref_r) __builtin_trap();
                break;
            }
            case OP_FIND: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                if ((em_it != em.end()) != (ref_it != ref.end())) __builtin_trap();
                if (em_it != em.end() && ref_it != ref.end())
                    if (em_it->second != ref_it->second) __builtin_trap();
                break;
            }
            case OP_ACCESS: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                if (em[op.key] != op.value) __builtin_trap();
                break;
            }
            case OP_ITERATE: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect[it->first] = it->second;
                if (em_collect != ref) __builtin_trap();
                break;
            }
            case OP_CLEAR: {
                em.clear();
                ref.clear();
                break;
            }
            case OP_RESERVE: {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 100000) em.reserve(cap);
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                if (em[op.key] != op.value) __builtin_trap();
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
                if (em.count(op.key) != ref.count(op.key)) __builtin_trap();
                break;
            }
            case OP_CONTAINS: {
                bool em_has = em.contains(op.key);
                bool ref_has = (ref.find(op.key) != ref.end());
                if (em_has != ref_has) __builtin_trap();
                break;
            }
            case OP_EMPLACE: {
                auto em_r = em.emplace(op.key, op.value);
                auto ref_r = ref.emplace(op.key, op.value);
                if (em_r.second != ref_r.second) __builtin_trap();
                break;
            }
            case OP_SHRINK: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
            case OP_SWAP: {
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
        if (em.size() != ref.size()) __builtin_trap();
    }

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
            if (!em.contains(k)) __builtin_trap();
        }
        size_t count = 0;
        for (auto it = em.begin(); it != em.end(); ++it) {
            if (ref.count(*it) != 1) __builtin_trap();
            count++;
        }
        if (count != ref.size()) __builtin_trap();
    };

    for (const auto& op : ops) {
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert(op.key);
                auto ref_r = ref.insert(op.key);
                if (em_r.second != ref_r.second) __builtin_trap();
                break;
            }
            case OP_ERASE: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                if (em_r != ref_r) __builtin_trap();
                break;
            }
            case OP_FIND: {
                if ((em.find(op.key) != em.end()) != (ref.find(op.key) != ref.end())) __builtin_trap();
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
                if (em_collect != ref) __builtin_trap();
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
                if (em.count(op.key) != ref.count(op.key)) __builtin_trap();
                break;
            }
            case OP_CONTAINS: {
                if (em.contains(op.key) != (ref.count(op.key) > 0)) __builtin_trap();
                break;
            }
            case OP_EMPLACE: {
                auto em_r = em.emplace(op.key);
                auto ref_r = ref.emplace(op.key);
                if (em_r.second != ref_r.second) __builtin_trap();
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
        if (em.size() != ref.size()) __builtin_trap();
    }
    verify_all();
}

// ============================================================================
// Heavy erase/insert churn test (small key range, no collision hasher)
// ============================================================================
template<typename HashMapT>
static void fuzz_churn(const std::vector<Op>& ops) {
    HashMapT em(4, 0.95f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        int key = op.key % 20; // Only 20 possible keys - heavy churn
        switch (op.code % 5) {
            case 0: {
                auto em_r = em.insert({key, op.value});
                auto ref_r = ref.insert({key, op.value});
                if (em_r.second != ref_r.second) __builtin_trap();
                break;
            }
            case 1: {
                size_t em_r = em.erase(key);
                size_t ref_r = ref.erase(key);
                if (em_r != ref_r) __builtin_trap();
                break;
            }
            case 2: {
                em[key] = op.value;
                ref[key] = op.value;
                if (em[key] != op.value) __builtin_trap();
                break;
            }
            case 3: {
                auto em_it = em.find(key);
                auto ref_it = ref.find(key);
                if ((em_it != em.end()) != (ref_it != ref.end())) __builtin_trap();
                if (em_it != em.end() && ref_it != ref.end())
                    if (em_it->second != ref_it->second) __builtin_trap();
                break;
            }
            case 4: {
                if (em.size() > 0) em.reserve(em.size());
                break;
            }
        }
        if (em.size() != ref.size()) __builtin_trap();
    }

    // Full verify
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        if (it == em.end()) __builtin_trap();
        if (it->second != kv.second) __builtin_trap();
    }
    size_t count = 0;
    for (auto it = em.begin(); it != em.end(); ++it) {
        if (ref.count(it->first) != 1) __builtin_trap();
        if (ref[it->first] != it->second) __builtin_trap();
        count++;
    }
    if (count != ref.size()) __builtin_trap();
}

// ============================================================================
// Main fuzzer entry point - NO collision hasher, all versions
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    uint8_t test_selector = data[0] % 20;

    const uint8_t* op_data = data + 1;
    size_t op_size = size - 1;
    auto ops = parse_input(op_data, op_size);

    if (ops.size() > 500) ops.resize(500);

    switch (test_selector) {
        case 0:  fuzz_extreme_hashmap<emhash5::HashMap<int, int>>(ops); break;
        case 1:  fuzz_extreme_hashmap<emhash6::HashMap<int, int>>(ops); break;
        case 2:  fuzz_extreme_hashmap<emhash7::HashMap<int, int>>(ops); break;
        case 3:  fuzz_extreme_hashmap<emhash8::HashMap<int, int>>(ops); break;
        case 4:  fuzz_extreme_hashset<emhash8::HashSet<int>>(ops); break;
        case 5:  fuzz_churn<emhash5::HashMap<int, int>>(ops); break;
        case 6:  fuzz_churn<emhash6::HashMap<int, int>>(ops); break;
        case 7:  fuzz_churn<emhash7::HashMap<int, int>>(ops); break;
        case 8:  fuzz_churn<emhash8::HashMap<int, int>>(ops); break;
        case 9:  fuzz_extreme_hashmap<emhash5::HashMap<int, int>>(ops, 0.98f); break;
        case 10: fuzz_extreme_hashmap<emhash6::HashMap<int, int>>(ops, 0.98f); break;
        case 11: fuzz_extreme_hashmap<emhash7::HashMap<int, int>>(ops, 0.98f); break;
        case 12: fuzz_extreme_hashmap<emhash8::HashMap<int, int>>(ops, 0.98f); break;
        case 13: fuzz_extreme_hashmap<emhash8::HashMap<int, int>>(ops, 0.99f); break;
        case 14: fuzz_extreme_hashset<emhash8::HashSet<int>>(ops); break;
        // String key tests
        case 15: {
            emhash5::HashMap<std::string, int> em(2);
            std::unordered_map<std::string, int> ref;
            for (const auto& op : ops) {
                std::string key = std::to_string(op.key % 50);
                switch (op.code % 4) {
                    case 0: em[key] = op.value; ref[key] = op.value; break;
                    case 1: em.erase(key); ref.erase(key); break;
                    case 2: if ((em.find(key) != em.end()) != (ref.find(key) != ref.end())) __builtin_trap(); break;
                    case 3: if (em.size() > 0) em.reserve(em.size()); break;
                }
                if (em.size() != ref.size()) __builtin_trap();
            }
            break;
        }
        case 16: {
            emhash7::HashMap<std::string, int> em(2);
            std::unordered_map<std::string, int> ref;
            for (const auto& op : ops) {
                std::string key = std::to_string(op.key % 50);
                switch (op.code % 4) {
                    case 0: em[key] = op.value; ref[key] = op.value; break;
                    case 1: em.erase(key); ref.erase(key); break;
                    case 2: if ((em.find(key) != em.end()) != (ref.find(key) != ref.end())) __builtin_trap(); break;
                    case 3: if (em.size() > 0) em.reserve(em.size()); break;
                }
                if (em.size() != ref.size()) __builtin_trap();
            }
            break;
        }
        case 17: {
            emhash8::HashMap<std::string, int> em(2);
            std::unordered_map<std::string, int> ref;
            for (const auto& op : ops) {
                std::string key = std::to_string(op.key % 50);
                switch (op.code % 4) {
                    case 0: em[key] = op.value; ref[key] = op.value; break;
                    case 1: em.erase(key); ref.erase(key); break;
                    case 2: if ((em.find(key) != em.end()) != (ref.find(key) != ref.end())) __builtin_trap(); break;
                    case 3: if (em.size() > 0) em.reserve(em.size()); break;
                }
                if (em.size() != ref.size()) __builtin_trap();
            }
            break;
        }
        // emhash6 string
        case 18: {
            emhash6::HashMap<std::string, int> em(2);
            std::unordered_map<std::string, int> ref;
            for (const auto& op : ops) {
                std::string key = std::to_string(op.key % 50);
                switch (op.code % 4) {
                    case 0: em[key] = op.value; ref[key] = op.value; break;
                    case 1: em.erase(key); ref.erase(key); break;
                    case 2: if ((em.find(key) != em.end()) != (ref.find(key) != ref.end())) __builtin_trap(); break;
                    case 3: if (em.size() > 0) em.reserve(em.size()); break;
                }
                if (em.size() != ref.size()) __builtin_trap();
            }
            break;
        }
        // emhash8 HashSet churn
        case 19: {
            emhash8::HashSet<int> em(4);
            std::unordered_set<int> ref;
            for (const auto& op : ops) {
                int key = op.key % 20;
                switch (op.code % 4) {
                    case 0: em.insert(key); ref.insert(key); break;
                    case 1: { size_t e = em.erase(key); size_t r = ref.erase(key); if (e != r) __builtin_trap(); break; }
                    case 2: if (em.contains(key) != (ref.count(key) > 0)) __builtin_trap(); break;
                    case 3: if (em.size() > 0) em.reserve(em.size()); break;
                }
                if (em.size() != ref.size()) __builtin_trap();
            }
            break;
        }
    }

    return 0;
}
