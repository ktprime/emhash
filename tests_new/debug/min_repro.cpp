// Minimal reproduction for emhash8 chain corruption bug
// Reads a crash file and replays it
#include "hash_table8.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <fstream>

enum OpCode { OP_INSERT, OP_ERASE, OP_FIND, OP_ACCESS, OP_ITERATE, OP_CLEAR, OP_RESERVE, OP_INSERT_OR_ASSIGN, OP_ERASE_ITERATOR, OP_COUNT, OP_CONTAINS, OP_EMPLACE };

struct Op {
    OpCode code;
    int key;
    int value;
};

static std::vector<Op> parse_input(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    for (size_t i = 0; i + 3 <= size; i += 3) {
        uint8_t op = data[i] % 10;
        int16_t key = (int16_t)(data[i + 1] | (data[i + 2] << 8));
        ops.push_back({(OpCode)op, (int)key, (int)key * 3});
    }
    return ops;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s <crash_file>\n", argv[0]); return 1; }

    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    f.close();

    if (data.empty()) { printf("Empty file\n"); return 1; }

    uint8_t test_selector = data[0] % 10;
    const uint8_t* op_data = data.data() + 1;
    size_t op_size = data.size() - 1;

    printf("test_selector=%d data_size=%zu\n", test_selector, data.size());

    auto ops = parse_input(op_data, op_size);
    if (ops.size() > 500) ops.resize(500);

    printf("num_ops=%zu\n", ops.size());

    if (test_selector == 3) { // emhash8::HashMap
        emhash8::HashMap<int, int> em;
        std::unordered_map<int, int> ref;

        for (size_t i = 0; i < ops.size(); i++) {
            const auto& op = ops[i];
            printf("[%zu] op=%d key=%d | em.size=%u ref.size=%zu\n", i, op.code, op.key, em.size(), ref.size());

            switch (op.code) {
                case OP_INSERT: {
                    auto em_r = em.insert({op.key, op.value});
                    auto ref_r = ref.insert({op.key, op.value});
                    printf("  INSERT em_ok=%d ref_ok=%d\n", em_r.second, ref_r.second);
                    break;
                }
                case OP_ERASE: {
                    size_t em_r = em.erase(op.key);
                    size_t ref_r = ref.erase(op.key);
                    printf("  ERASE em=%zu ref=%zu\n", em_r, ref_r);
                    break;
                }
                case OP_FIND: {
                    auto em_it = em.find(op.key);
                    auto ref_it = ref.find(op.key);
                    printf("  FIND em=%d ref=%d\n", em_it != em.end(), ref_it != ref.end());
                    break;
                }
                case OP_ACCESS: {
                    em[op.key] = op.value;
                    ref[op.key] = op.value;
                    break;
                }
                case OP_ITERATE: {
                    size_t count = 0;
                    for (auto it = em.begin(); it != em.end(); ++it) count++;
                    printf("  ITERATE count=%zu\n", count);
                    break;
                }
                case OP_CLEAR: {
                    em.clear(); ref.clear();
                    break;
                }
                case OP_RESERVE: {
                    em.reserve(op.key > 0 ? op.key : 10);
                    break;
                }
                case OP_INSERT_OR_ASSIGN: {
                    int v = op.value;
                    em.insert_or_assign(op.key, std::move(v));
                    ref[op.key] = op.value;
                    break;
                }
                case OP_COUNT: {
                    printf("  COUNT em=%zu ref=%zu\n", em.count(op.key), ref.count(op.key));
                    break;
                }
                case OP_CONTAINS: {
                    printf("  CONTAINS em=%d ref=%d\n", em.contains(op.key), ref.count(op.key) > 0);
                    break;
                }
                case OP_EMPLACE: {
                    auto em_r = em.emplace(op.key, op.value);
                    auto ref_r = ref.emplace(op.key, op.value);
                    printf("  EMPLACE em_ok=%d ref_ok=%d\n", em_r.second, ref_r.second);
                    break;
                }
                default: break;
            }

            if (em.size() != ref.size()) {
                printf("SIZE MISMATCH! em=%u ref=%zu after op[%zu]=%d key=%d\n", em.size(), ref.size(), i, op.code, op.key);
                for (auto& kv : ref) {
                    auto it = em.find(kv.first);
                    printf("  ref_key=%d em_found=%d\n", kv.first, it != em.end());
                }
                abort();
            }
        }
    }

    printf("Done\n");
    return 0;
}
