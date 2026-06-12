// Fuzzing test for emhash8 using libFuzzer
// Compile with: clang++ -fsanitize=fuzzer,address -std=c++17 -g fuzz_emhash8.cpp -o fuzz_emhash8
// Or with coverage only: clang++ -fsanitize=fuzzer -std=c++17 -g fuzz_emhash8.cpp -o fuzz_emhash8
//
// Run:
//   ./fuzz_emhash8 -max_total_time=60 -print_final_stats=1
//   ./fuzz_emhash8 corpus/ -max_total_time=300

#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "../hash_table8.hpp"

// We interpret fuzz input as a sequence of operations
// Each operation: [op_code(1 byte), key(4 bytes), value(4 bytes)]
// op_code: 0=insert, 1=erase, 2=find, 3=operator[], 4=iterate, 5=clear, 6=reserve

enum OpCode : uint8_t {
    OP_INSERT = 0,
    OP_ERASE = 1,
    OP_FIND = 2,
    OP_ACCESS = 3,    // operator[]
    OP_ITERATE = 4,
    OP_CLEAR = 5,
    OP_RESERVE = 6,
    OP_INSERT_OR_ASSIGN = 7,
    OP_ERASE_ITERATOR = 8,
    OP_COUNT = 9,
};

struct Op {
    OpCode code;
    int key;
    int value;
};

static std::vector<Op> parse_input(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= size) {
        Op op;
        op.code = static_cast<OpCode>(data[i] % 10);
        // Read key (little-endian)
        op.key = *reinterpret_cast<const int32_t*>(data + i + 1);
        // Read value
        op.value = *reinterpret_cast<const int32_t*>(data + i + 5);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

// Test 1: Compare emhash8 against std::unordered_map for correctness
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    auto ops = parse_input(data, size);
    
    emhash8::HashMap<int, int> em;
    std::unordered_map<int, int> ref;
    
    std::vector<emhash8::HashMap<int, int>::iterator> em_iters;
    std::vector<std::unordered_map<int, int>::iterator> ref_iters;
    
    for (const auto& op : ops) {
        switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                // Both should agree on whether insertion happened
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
                // operator[]
                int& em_v = em[op.key];
                int& ref_v = ref[op.key];
                em_v = op.value;
                ref_v = op.value;
                break;
            }
            case OP_ITERATE: {
                // Collect all elements and compare
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
                if (cap < 1000000) {  // Avoid huge allocations
                    em.reserve(cap);
                    ref.reserve(cap);
                }
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                auto em_r = em.insert_or_assign(op.key, op.value);
                auto ref_it = ref.find(op.key);
                bool ref_inserted = (ref_it == ref.end());
                ref[op.key] = op.value;
                assert(em_r.second == ref_inserted);
                break;
            }
            case OP_ERASE_ITERATOR: {
                // Erase using iterator (if exists)
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
        }
        
        // Invariant: sizes must match
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
    
    return 0;
}
