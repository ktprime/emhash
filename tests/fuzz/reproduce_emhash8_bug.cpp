// Minimal reproduction test for emhash8 bug found by fuzz_emhash8_advanced
// Bug: em.size() != ref.size() after certain operations
// Crash input: \000\001\001\003\220\000\001\001\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000

#include <cstdint>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "emhash/hash_table8.hpp"

// Custom hasher that allows controlled collisions (same as fuzz test)
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
};

int main() {
    std::cout << "=== Reproducing emhash8 bug ===\n";
    
    // Parse crash input manually
    // Input: \000\001\001\003\220\000\001\001\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000
    // First 8 bytes = seed
    // Then 9-byte ops: code(1) + key(4) + value(4)
    
    uint8_t crash_data[] = {0x00, 0x01, 0x01, 0x03, 0x90, 0x00, 0x01, 0x01, 
                            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                            0x00, 0x00};
    
    // Seed from first 8 bytes
    uint64_t seed = *reinterpret_cast<const uint64_t*>(crash_data);
    std::cout << "Seed: " << seed << " (0x" << std::hex << seed << std::dec << ")\n";
    
    // Parse operations
    struct Op { uint8_t code; int32_t key; int32_t value; };
    std::vector<Op> ops;
    for (size_t i = 8; i + 9 <= sizeof(crash_data); i += 9) {
        Op op;
        op.code = crash_data[i];
        op.key = *reinterpret_cast<const int32_t*>(crash_data + i + 1);
        op.value = *reinterpret_cast<const int32_t*>(crash_data + i + 5);
        ops.push_back(op);
        std::cout << "Op[" << ops.size()-1 << "]: code=" << op.code 
                  << " (cmd=" << (op.code % 12) << ")"
                  << ", key=" << op.key << ", value=" << op.value << "\n";
    }
    
    // Create maps with same settings as fuzz test
    emhash8::HashMap<int, int, FuzzHasher> em(2, 0.8f);
    std::unordered_map<int, int> ref;
    
    // Execute operations
    std::cout << "\n=== Executing operations ===\n";
    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        uint8_t cmd = op.code % 12;
        
        std::cout << "Step " << i << ": cmd=" << cmd;
        
        switch (cmd) {
            case 0: {  // insert_unique
                std::cout << " (insert_unique key=" << op.key << " val=" << op.value << ")";
                auto em_r = em.insert_unique(op.key, op.value);
                auto ref_r = ref.insert({op.key, op.value});
                std::cout << " em.bucket=" << em_r << " ref.inserted=" << ref_r.second;
                break;
            }
            case 1: {  // erase by key
                std::cout << " (erase key=" << op.key << ")";
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                std::cout << " em.erased=" << em_r << " ref.erased=" << ref_r;
                if (em_r != ref_r) {
                    std::cout << " *** MISMATCH!";
                }
                break;
            }
            case 2: {  // find
                std::cout << " (find key=" << op.key << ")";
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                bool em_found = (em_it != em.end());
                bool ref_found = (ref_it != ref.end());
                std::cout << " em.found=" << em_found << " ref.found=" << ref_found;
                break;
            }
            case 3: {  // operator[]
                std::cout << " (operator[] key=" << op.key << " val=" << op.value << ")";
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 7: {  // insert_or_assign
                std::cout << " (insert_or_assign key=" << op.key << " val=" << op.value << ")";
                int v = op.value;
                auto em_r = em.insert_or_assign(op.key, std::move(v));
                bool was_new = (ref.find(op.key) == ref.end());
                ref[op.key] = op.value;
                std::cout << " em.inserted=" << em_r.second << " was_new=" << was_new;
                break;
            }
            default:
                std::cout << " (cmd=" << cmd << " skipped)";
                break;
        }
        
        std::cout << " | em.size=" << em.size() << " ref.size=" << ref.size();
        
        // Check the invariant
        if (em.size() != ref.size()) {
            std::cout << "\n\n*** BUG FOUND: em.size() != ref.size() ***\n";
            std::cout << "em.size() = " << em.size() << "\n";
            std::cout << "ref.size() = " << ref.size() << "\n";
            std::cout << "Difference = " << (em.size() - ref.size()) << "\n";
            
            // Debug: print all elements in em
            std::cout << "\nemhash8 contents:\n";
            for (auto it = em.begin(); it != em.end(); ++it) {
                std::cout << "  [" << it->first << "] = " << it->second << "\n";
            }
            
            std::cout << "\nref contents:\n";
            for (const auto& kv : ref) {
                std::cout << "  [" << kv.first << "] = " << kv.second << "\n";
            }
            
            return 1;
        }
        
        std::cout << "\n";
    }
    
    std::cout << "\n=== Test passed (no bug found with this input) ===\n";
    return 0;
}