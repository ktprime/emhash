// Debug emilib2o crash - reproduce and analyze
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <cassert>

#include "emilib/emilib2o.hpp"

struct Op {
    uint8_t code;
    int32_t key;
    int32_t value;
};

static std::vector<Op> parse_input(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= size) {
        Op op;
        op.code = static_cast<uint8_t>(data[i] % 12);
        memcpy(&op.key, data + i + 1, 4);
        memcpy(&op.value, data + i + 5, 4);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

int main() {
    // The crash input
    uint8_t data[] = {0x8a, 0xfb, 0x6f, 0x6f, 0xff, 0xff, 0xff, 0x6f, 0xff, 0xff, 0x17, 0xff, 0xff, 0x00, 0x6f, 0x6f, 0x6f, 0xff, 0xff, 0xff, 0x0b, 0x85};

    uint8_t test_selector = data[0] % 15;
    printf("test_selector = %d (should be 3 for emilib2o)\n", test_selector);

    const uint8_t* op_data = data + 1;
    size_t op_size = sizeof(data) - 1;

    auto ops = parse_input(op_data, op_size);
    printf("num ops = %zu\n", ops.size());

    // test_selector 3 = fuzz_hashmap<emilib2::HashMap<int, int>>
    emilib2::HashMap<int, int> em(4, 0.8f);
    std::unordered_map<int, int> ref;

    for (size_t i = 0; i < ops.size(); i++) {
        const auto& op = ops[i];
        printf("op[%zu]: code=%d key=%d value=%d | em.size=%zu ref.size=%zu\n",
               i, op.code, op.key, op.value, em.size(), ref.size());

        switch (op.code) {
            case 0: { // INSERT
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                if (em_r.second != ref_r.second) {
                    printf("  MISMATCH: insert(%d) em.second=%d ref.second=%d\n",
                           op.key, em_r.second, ref_r.second);
                }
                break;
            }
            case 1: { // ERASE
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                if (em_r != ref_r) {
                    printf("  MISMATCH: erase(%d) em=%zu ref=%zu\n", op.key, em_r, ref_r);
                }
                break;
            }
            case 2: { // FIND
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                bool em_found = (em_it != em.end());
                bool ref_found = (ref_it != ref.end());
                if (em_found != ref_found) {
                    printf("  MISMATCH: find(%d) em_found=%d ref_found=%d\n", op.key, em_found, ref_found);
                }
                break;
            }
            case 3: { // ACCESS
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 4: { // ITERATE
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it) {
                    em_collect[it->first] = it->second;
                }
                if (em_collect != ref) {
                    printf("  MISMATCH: iterate em_collect.size=%zu ref.size=%zu\n",
                           em_collect.size(), ref.size());
                }
                break;
            }
            case 5: { // CLEAR
                em.clear();
                ref.clear();
                break;
            }
            case 6: { // RESERVE
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 1000000) {
                    em.reserve(cap);
                    ref.reserve(cap);
                }
                break;
            }
            case 7: { // INSERT_OR_ASSIGN
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                break;
            }
            case 8: { // ERASE_ITERATOR
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                }
                break;
            }
            case 9: { // COUNT
                if (em.count(op.key) != ref.count(op.key)) {
                    printf("  MISMATCH: count(%d) em=%zu ref=%zu\n", op.key, em.count(op.key), ref.count(op.key));
                }
                break;
            }
            case 10: { // CONTAINS
                bool em_has = em.contains(op.key);
                bool ref_has = (ref.find(op.key) != ref.end());
                if (em_has != ref_has) {
                    printf("  MISMATCH: contains(%d) em=%d ref=%d\n", op.key, em_has, ref_has);
                }
                break;
            }
            case 11: { // EMPLACE
                auto em_r = em.emplace(op.key, op.value);
                auto ref_r = ref.emplace(op.key, op.value);
                if (em_r.second != ref_r.second) {
                    printf("  MISMATCH: emplace(%d) em.second=%d ref.second=%d\n",
                           op.key, em_r.second, ref_r.second);
                }
                break;
            }
        }
        if (em.size() != ref.size()) {
            printf("  SIZE MISMATCH after op[%zu]: em=%zu ref=%zu\n", i, em.size(), ref.size());
        }
    }

    printf("\nFinal verification:\n");
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        if (it == em.end()) {
            printf("  BUG: key=%d exists in ref but not in em!\n", kv.first);
        } else if (it->second != kv.second) {
            printf("  BUG: key=%d value mismatch: em=%d ref=%d\n", kv.first, it->second, kv.second);
        }
    }
    for (auto it = em.begin(); it != em.end(); ++it) {
        if (ref.count(it->first) != 1) {
            printf("  BUG: key=%d exists in em but not in ref!\n", it->first);
        } else if (ref[it->first] != it->second) {
            printf("  BUG: key=%d value mismatch: em=%d ref=%d\n", it->first, it->second, ref[it->first]);
        }
    }

    printf("Done.\n");
    return 0;
}
