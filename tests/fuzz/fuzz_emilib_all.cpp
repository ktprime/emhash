// fuzz/fuzz_emilib_all.cpp
// Unified libfuzzer target for emilib::HashMap (emihmap1/2/3).
// Parses fuzzer input into operation stream, runs against std::unordered_map
// oracle, asserts equivalence.
//
// Build (libfuzzer): clang++ -fsanitize=fuzzer,address -std=c++17 \
//                         -I../include -Icommon fuzz_emilib_all.cpp
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>
#include <unordered_map>

#include "emilib/emihmap1.hpp"
#include "emilib/emihmap2.hpp"
#include "emilib/emihmap3.hpp"

// ============================================================================
// Operation stream parsed from fuzzer input
// ============================================================================
enum OpCode : uint8_t {
    OP_INSERT = 0,
    OP_ERASE = 1,
    OP_FIND = 2,
    OP_ACCESS = 3,
    OP_ITERATE = 4,
    OP_CLEAR = 5,
    OP_RESERVE = 6,
    OP_EMPLACE = 7,
    OP_COUNT = 8,
    OP_CONTAINS = 9,
    OP_MAX = 10
};

struct Op {
    OpCode code;
    int32_t key;
    int32_t value;
};

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
// Fuzz a single emilib HashMap type against oracle
// ============================================================================
template <typename MapT> static void fuzz_map(const std::vector<Op>& ops) {
    MapT m;
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
            for (auto it = m.begin(); it != m.end(); ++it)
                ++count;
            assert(count == m.size());
            break;
        }
        case OP_CLEAR:
            m.clear();
            ref.clear();
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
        default:
            break;
        }
    }
    assert(m.size() == ref.size());
}

// ============================================================================
// libfuzzer entry: cycle through emilib1/2/3
// ============================================================================
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 9)
        return 0;
    auto ops = parse_ops(data, size);
    if (ops.empty())
        return 0;

    uint8_t selector = data[0] % 3;
    switch (selector) {
    case 0:
        fuzz_map<emilib::HashMap<int, int>>(ops);
        break;
    case 1:
        fuzz_map<emilib2::HashMap<int, int>>(ops);
        break;
    case 2:
        fuzz_map<emilib3::HashMap<int, int>>(ops);
        break;
    }
    return 0;
}
