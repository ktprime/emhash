// Debug emilib2ss key=127 find failure - minimal reproduction
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "emilib/emilib2ss.hpp"

struct Op {
    uint8_t op;
    int key;
    int value;
};

static void parse_ops(const uint8_t* data, size_t size, std::vector<Op>& ops) {
    size_t i = 0;
    while (i + 5 <= size) {
        Op op;
        op.op = data[i] % 4;
        op.key = (int)(data[i+1] | (data[i+2] << 8) | (data[i+3] << 16) | (data[i+4] << 24));
        op.value = op.key;
        ops.push_back(op);
        i += 5;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <crash_file>\n", argv[0]);
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    std::vector<Op> ops;
    parse_ops(data.data(), size, ops);

    emilib::HashMap<int, int> em(4);
    std::unordered_map<int, int> ref;

    for (size_t idx = 0; idx < ops.size(); idx++) {
        const auto& op = ops[idx];

        // Before the operation, check if key=127 exists
        bool had_127 = ref.count(127) > 0;

        switch (op.op) {
            case 0: { // insert
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
            case 1: { // erase
                auto it = em.find(op.key);
                if (it != em.end()) em.erase(it);
                ref.erase(op.key);
                break;
            }
            case 2: break;
            case 3: {
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }

        // After the operation, check key=127
        bool has_127 = ref.count(127) > 0;
        if (has_127) {
            auto it = em.find(127);
            if (it == em.end()) {
                fprintf(stderr, "FAIL at op[%zu] type=%d key=%d: key=127 lost! had_127=%d has_127=%d em.size=%u ref.size=%zu\n",
                    idx, op.op, op.key, had_127, has_127, (unsigned)em.size(), ref.size());

                // Try to find 127 by iterating
                bool found_by_iter = false;
                for (auto& kv : em) {
                    if (kv.first == 127) {
                        found_by_iter = true;
                        fprintf(stderr, "  key=127 found by iteration! value=%d\n", kv.second);
                        break;
                    }
                }
                fprintf(stderr, "  found_by_iter=%d\n", found_by_iter);
                return 1;
            }
        }
    }

    printf("All %zu ops passed\n", ops.size());
    return 0;
}
