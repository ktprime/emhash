// Analyze crash-1909d956d6d9d6a7fa8a4145a9532469e499d0cc
#include <cstdint>
#include <cstdio>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <cassert>
#include "../hash_table8.hpp"

enum OpCode : uint8_t {
    OP_INSERT = 0, OP_ERASE = 1, OP_FIND = 2,
    OP_ACCESS = 3, OP_ITERATE = 4, OP_CLEAR = 5,
    OP_RESERVE = 6, OP_INSERT_OR_ASSIGN = 7,
    OP_ERASE_ITERATOR = 8, OP_COUNT = 9,
};

struct Op { OpCode code; int key; int value; };

int main() {
    std::ifstream f("../fuzz/crash-1909d956d6d9d6a7fa8a4145a9532469e499d0cc", std::ios::binary);
    if (!f) { printf("cannot open crash file\n"); return 1; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    printf("Crash file size: %zu bytes\n", data.size());

    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= data.size()) {
        Op op;
        op.code = static_cast<OpCode>(data[i] % 10);
        op.key = *reinterpret_cast<const int32_t*>(&data[i+1]);
        op.value = *reinterpret_cast<const int32_t*>(&data[i+5]);
        ops.push_back(op);
        i += 9;
    }

    emhash8::HashMap<int, int> em;
    std::unordered_map<int, int> ref;
    printf("Replaying %zu operations...\n", ops.size());
    for (size_t j = 0; j < ops.size(); ++j) {
        const auto& op = ops[j];
        printf("%3zu: ", j);
        switch (op.code) {
            case OP_INSERT: printf("insert(%d, %d)\n", op.key, op.value);
                em.insert({op.key, op.value}); ref.insert({op.key, op.value}); break;
            case OP_ERASE:  printf("erase(%d)\n", op.key);
                em.erase(op.key); ref.erase(op.key); break;
            case OP_FIND:   printf("find(%d)\n", op.key);
                em.find(op.key); ref.find(op.key); break;
            case OP_ACCESS: printf("operator[](%d) = %d\n", op.key, op.value);
                em[op.key] = op.value; ref[op.key] = op.value; break;
            case OP_ITERATE: printf("iterate\n"); {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it) em_collect[it->first] = it->second;
                assert(em_collect == ref);
                break;
            }
            case OP_CLEAR:  printf("clear()\n"); em.clear(); ref.clear(); break;
            case OP_RESERVE: printf("reserve(%d)\n", op.key); {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 1000000) { em.reserve(cap); ref.reserve(cap); }
                break;
            }
            case OP_INSERT_OR_ASSIGN: printf("insert_or_assign(%d, %d)\n", op.key, op.value); {
                int v = op.value; em.insert_or_assign(op.key, std::move(v)); ref[op.key] = op.value; break;
            }
            case OP_ERASE_ITERATOR: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                printf("erase_iterator(%d) -> em=%s ref=%s\n", op.key, em_it != em.end() ? "found" : "not found", ref_it != ref.end() ? "found" : "not found");
                if (em_it != em.end() && ref_it != ref.end()) { em.erase(em_it); ref.erase(ref_it); }
                break;
            }
            case OP_COUNT: printf("count(%d)\n", op.key);
                assert(em.count(op.key) == ref.count(op.key)); break;
            default: printf("unknown op %d\n", (int)op.code); break;
        }
        assert(em.size() == ref.size());
    }
    printf("Replay finished without crash?\n");
    return 0;
}
