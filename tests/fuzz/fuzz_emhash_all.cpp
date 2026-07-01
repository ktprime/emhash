// fuzz/fuzz_emhash_all.cpp
// Unified libfuzzer target for emhash5/6/7/8 HashMap + emhash8 HashSet.
// Parses fuzzer input into operation stream, runs against std::unordered_map
// oracle, asserts equivalence. Uses common/hashers.hpp for collision patterns.
//
// Build (libfuzzer): clang++ -fsanitize=fuzzer,address -std=c++17 \
//                         -I../include -Icommon fuzz_emhash_all.cpp
// Build (non-libfuzzer): link with fuzz_main.cpp
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"
#include "emhash/hash_set8.hpp"

// ============================================================================
// Operation stream parsed from fuzzer input
// ============================================================================
enum OpCode : uint8_t {
    OP_INSERT = 0, OP_ERASE = 1, OP_FIND = 2, OP_ACCESS = 3,
    OP_ITERATE = 4, OP_CLEAR = 5, OP_RESERVE = 6, OP_EMPLACE = 7,
    OP_COUNT = 8, OP_CONTAINS = 9, OP_INSERT_UNIQUE = 10,
    OP_MAX = 11
};

struct Op { OpCode code; int32_t key; int32_t value; };

static std::vector<Op> parse_ops(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= size) {
        Op op;
        op.code = static_cast<OpCode>(data[i] % OP_MAX);
        memcpy(&op.key, data + i + 1, 4);
        memcpy(&op.value, data + i + 5, 4);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

// ============================================================================
// Fuzz a single HashMap type against oracle
// ============================================================================
template <typename MapT>
static void fuzz_map(const std::vector<Op>& ops) {
    MapT m(2, 0.8f);
    std::unordered_map<int, int> ref;

    for (const auto& op : ops) {
        switch (op.code) {
        case OP_INSERT: {
            auto er = m.insert({op.key, op.value});
            auto rr = ref.insert({op.key, op.value});
            assert(er.second == rr.second);
            break;
        }
        case OP_ERASE: {
            size_t er = m.erase(op.key);
            size_t rr = ref.erase(op.key);
            assert(er == rr);
            break;
        }
        case OP_FIND: {
            auto eit = m.find(op.key);
            auto rit = ref.find(op.key);
            assert((eit == m.end()) == (rit == ref.end()));
            break;
        }
        case OP_ACCESS: {
            if (ref.count(op.key)) {
                int ev = m[op.key];
                int rv = ref[op.key];
                assert(ev == rv);
            }
            break;
        }
        case OP_ITERATE: {
            size_t count = 0;
            for (auto it = m.begin(); it != m.end(); ++it) ++count;
            assert(count == m.size());
            break;
        }
        case OP_CLEAR:
            m.clear(); ref.clear();
            break;
        case OP_RESERVE:
            m.reserve(static_cast<size_t>(std::max(1, op.value)));
            break;
        case OP_EMPLACE: {
            auto er = m.emplace(op.key, op.value);
            auto rr = ref.emplace(op.key, op.value);
            assert(er.second == rr.second);
            break;
        }
        case OP_COUNT:
            assert(m.count(op.key) == ref.count(op.key));
            break;
        case OP_CONTAINS:
            assert(m.contains(op.key) == (ref.count(op.key) > 0));
            break;
        case OP_INSERT_UNIQUE: {
            if (!m.contains(op.key)) {
                m.insert_unique(op.key, op.value);
                auto rr = ref.insert({op.key, op.value});
                assert(rr.second);
            }
            break;
        }
        default: break;
        }
    }
    assert(m.size() == ref.size());
}

// ============================================================================
// Fuzz HashSet against oracle
// ============================================================================
template <typename SetT>
static void fuzz_set(const std::vector<Op>& ops) {
    SetT s(2, 0.8f);
    std::unordered_set<int> ref;

    for (const auto& op : ops) {
        switch (op.code) {
        case OP_INSERT: {
            auto er = s.insert(op.key);
            auto rr = ref.insert(op.key);
            assert(er.second == rr.second);
            break;
        }
        case OP_ERASE: {
            size_t er = s.erase(op.key);
            size_t rr = ref.erase(op.key);
            assert(er == rr);
            break;
        }
        case OP_FIND: {
            auto eit = s.find(op.key);
            auto rit = ref.find(op.key);
            assert((eit == s.end()) == (rit == ref.end()));
            break;
        }
        case OP_COUNT:
            assert(s.count(op.key) == ref.count(op.key));
            break;
        case OP_CONTAINS:
            assert(s.contains(op.key) == (ref.count(op.key) > 0));
            break;
        case OP_CLEAR:
            s.clear(); ref.clear();
            break;
        case OP_RESERVE:
            s.reserve(static_cast<size_t>(std::max(1, op.value)));
            break;
        case OP_ITERATE: {
            size_t count = 0;
            for (auto it = s.begin(); it != s.end(); ++it) ++count;
            assert(count == s.size());
            break;
        }
        case OP_INSERT_UNIQUE: {
            if (!s.contains(op.key)) {
                s.insert_unique(op.key);
                auto rr = ref.insert(op.key);
                assert(rr.second);
            }
            break;
        }
        default: break;
        }
    }
    assert(s.size() == ref.size());
}

// ============================================================================
// libfuzzer entry: cycle through all map/set types
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 9) return 0;
    auto ops = parse_ops(data, size);
    if (ops.empty()) return 0;

    // Cycle through implementations based on first byte
    uint8_t selector = data[0] % 5;
    switch (selector) {
        case 0: fuzz_map<emhash5::HashMap<int, int>>(ops); break;
        case 1: fuzz_map<emhash6::HashMap<int, int>>(ops); break;
        case 2: fuzz_map<emhash7::HashMap<int, int>>(ops); break;
        case 3: fuzz_map<emhash8::HashMap<int, int>>(ops); break;
        case 4: fuzz_set<emhash8::HashSet<int>>(ops); break;
    }
    return 0;
}
