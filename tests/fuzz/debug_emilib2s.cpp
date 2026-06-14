// Debug emilib2s (emilib3) crash
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "emilib/emilib2s.hpp"

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

int main(int argc, char** argv) {
    // Read crash file
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("Usage: %s <crash_file>\n", argv[0]); return 1; }
    uint8_t data[4096];
    size_t size = fread(data, 1, sizeof(data), f);
    fclose(f);

    uint8_t test_selector = data[0] % 15;
    printf("test_selector = %d\n", test_selector);

    const uint8_t* op_data = data + 1;
    size_t op_size = size - 1;

    auto ops = parse_input(op_data, op_size);
    if (ops.size() > 500) ops.resize(500);
    printf("num ops = %zu\n", ops.size());

    // test_selector 4 = fuzz_high_load<emilib3::HashMap<int, int>>
    emilib3::HashMap<int, int> em(4, 0.95f);
    std::unordered_map<int, int> ref;

    for (size_t i = 0; i < ops.size(); i++) {
        const auto& op = ops[i];
        bool is_target = (op.key == 713095937 || op.key == 7950);
        if (is_target) printf(">>> op[%zu] code=%d key=%d value=%d em.size=%u\n", i, op.code % 4, op.key, op.value, em.size());
        switch (op.code % 4) {
            case 0: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                if (em_r.second != ref_r.second) {
                    printf("MISMATCH at op[%zu] insert(%d): em=%d ref=%d\n",
                           i, op.key, em_r.second, ref_r.second);
                }
                break;
            }
            case 1: {
                size_t em_r = em.erase(op.key);
                size_t ref_r = ref.erase(op.key);
                if (em_r != ref_r) {
                    printf("MISMATCH at op[%zu] erase(%d): em=%zu ref=%zu\n",
                           i, op.key, em_r, ref_r);
                }
                break;
            }
            case 2: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                bool em_found = (em_it != em.end());
                bool ref_found = (ref_it != ref.end());
                if (em_found != ref_found) {
                    printf("MISMATCH at op[%zu] find(%d): em=%d ref=%d\n",
                           i, op.key, em_found, ref_found);
                }
                break;
            }
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }
        if (em.size() != ref.size()) {
            printf("SIZE MISMATCH at op[%zu]: em=%u ref=%zu\n", i, em.size(), ref.size());
        }
    }

    printf("\nFinal verification:\n");
    int bugs = 0;
    for (const auto& kv : ref) {
        auto it = em.find(kv.first);
        if (it == em.end()) {
            printf("BUG: key=%d in ref but not in em\n", kv.first);
            bugs++;
        } else if (it->second != kv.second) {
            printf("BUG: key=%d value mismatch: em=%d ref=%d\n", kv.first, it->second, kv.second);
            bugs++;
        }
    }
    if (bugs == 0) printf("All OK!\n");
    return bugs;
}
