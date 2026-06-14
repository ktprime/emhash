// Debug emilib2ss find failure - detailed trace
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
            case 2: { // find - no-op
                break;
            }
            case 3: { // access
                em[op.key] = op.value;
                ref[op.key] = op.value;
                break;
            }
        }

        // Verify after every operation
        for (const auto& kv : ref) {
            auto it = em.find(kv.first);
            if (it == em.end()) {
                fprintf(stderr, "FAIL at op[%zu] type=%d key=%d: find returns end() (em.size=%zu ref.size=%zu)\n",
                    idx, op.op, kv.first, em.size(), ref.size());
                // Print emilib internal state
                fprintf(stderr, "  em bucket_count=%zu\n", em.bucket_count());
                return 1;
            }
        }
    }

    printf("All %zu ops passed, %zu keys verified\n", ops.size(), ref.size());
    return 0;
}
