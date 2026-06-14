// Debug emilib2ss find failure
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cassert>
#include <algorithm>

#include "emilib/emilib2ss.hpp"

struct Op {
    uint8_t op;  // 0=insert, 1=erase, 2=find, 3=access
    int key;
    int value;
};

static void parse_ops(const uint8_t* data, size_t size, std::vector<Op>& ops) {
    size_t i = 0;
    while (i + 5 <= size) {
        Op op;
        op.op = data[i] % 4;
        op.key = (int)(data[i+1] | (data[i+2] << 8) | (data[i+3] << 16) | (data[i+4] << 24));
        op.value = op.key * 2;
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
    std::vector<std::pair<int,int>> inserted; // track what's in the map

    for (size_t idx = 0; idx < ops.size(); idx++) {
        const auto& op = ops[idx];
        switch (op.op) {
            case 0: { // insert
                auto result = em.emplace(op.key, op.value);
                if (result.second) {
                    inserted.push_back({op.key, op.value});
                } else {
                    result.first->second = op.value;
                    for (auto& p : inserted) {
                        if (p.first == op.key) { p.second = op.value; break; }
                    }
                }
                break;
            }
            case 1: { // erase
                auto it = em.find(op.key);
                if (it != em.end()) {
                    em.erase(it);
                    inserted.erase(std::remove_if(inserted.begin(), inserted.end(),
                        [&](const auto& p) { return p.first == op.key; }), inserted.end());
                }
                break;
            }
            case 2: { // find
                auto it = em.find(op.key);
                // just check
                break;
            }
            case 3: { // access
                em[op.key] = op.value;
                bool found = false;
                for (auto& p : inserted) {
                    if (p.first == op.key) { p.second = op.value; found = true; break; }
                }
                if (!found) inserted.push_back({op.key, op.value});
                break;
            }
        }
    }

    // Verify all inserted keys are findable
    int fail_count = 0;
    for (const auto& kv : inserted) {
        auto it = em.find(kv.first);
        if (it == em.end()) {
            fprintf(stderr, "FAIL: key=%d inserted but find() returns end()\n", kv.first);
            fail_count++;
            if (fail_count > 10) break;
        } else if (it->second != kv.second) {
            fprintf(stderr, "FAIL: key=%d expected value=%d got value=%d\n", kv.first, kv.second, it->second);
            fail_count++;
            if (fail_count > 10) break;
        }
    }

    if (fail_count == 0) {
        printf("All %zu keys verified OK\n", inserted.size());
    } else {
        printf("FAILED %d keys out of %zu\n", fail_count, inserted.size());
    }

    return fail_count > 0 ? 1 : 0;
}
