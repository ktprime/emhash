// Debug emhash8 HashSet erase_slot chain corruption
// Compile: clang++-20 -fsanitize=address -std=c++17 -g -O0 -I. fuzz/debug_set_erase.cpp -o fuzz/debug_set_erase
#include "emhash/hash_set8.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <vector>
#include <fstream>

enum OpCode : uint8_t {
    OP_INSERT = 0,
    OP_ERASE = 1,
    OP_FIND = 2,
    OP_ACCESS = 3,
    OP_ITERATE = 4,
    OP_CLEAR = 5,
    OP_RESERVE = 6,
    OP_INSERT_OR_ASSIGN = 7,
    OP_ERASE_ITERATOR = 8,
    OP_COUNT = 9,
    OP_CONTAINS = 10,
    OP_EMPLACE = 11,
};

struct Op {
    OpCode code;
    int32_t key;
    int32_t value;
};

static std::vector<Op> parse_input(const uint8_t* data, size_t size) {
    std::vector<Op> ops;
    size_t i = 0;
    while (i + 9 <= size) {
        Op op;
        op.code = static_cast<OpCode>(data[i] % 12);
        memcpy(&op.key, data + i + 1, 4);
        memcpy(&op.value, data + i + 5, 4);
        ops.push_back(op);
        i += 9;
    }
    return ops;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <crash_file>\n", argv[0]);
        return 1;
    }

    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    f.close();

    if (data.empty()) {
        fprintf(stderr, "Empty file\n");
        return 1;
    }

    uint8_t test_selector = data[0] % 9;
    const uint8_t* op_data = data.data() + 1;
    size_t op_size = data.size() - 1;

    fprintf(stderr, "test_selector=%d data_size=%zu\n", test_selector, data.size());

    if (test_selector != 4) {
        fprintf(stderr, "Not a HashSet test (selector=%d), skipping\n", test_selector);
        return 0;
    }

    auto ops = parse_input(op_data, op_size);
    if (ops.size() > 500)
        ops.resize(500);
    fprintf(stderr, "num_ops=%zu\n", ops.size());

    emhash8::HashSet<int> em(2);
    std::unordered_set<int> ref;

    for (size_t i = 0; i < ops.size(); i++) {
        const auto& op = ops[i];
        fprintf(stderr, "[%zu] op=%d key=%d | em.size=%u ref.size=%zu\n", i, (int)op.code, op.key, (unsigned)em.size(),
                ref.size());

        switch (op.code) {
        case OP_INSERT: {
            auto em_r = em.insert(op.key);
            auto ref_r = ref.insert(op.key);
            fprintf(stderr, "  INSERT key=%d em_ok=%d ref_ok=%d\n", op.key, em_r.second, ref_r.second);
            if (em_r.second != ref_r.second) {
                fprintf(stderr, "  MISMATCH!\n");
                abort();
            }
            break;
        }
        case OP_ERASE: {
            size_t em_r = em.erase(op.key);
            size_t ref_r = ref.erase(op.key);
            fprintf(stderr, "  ERASE key=%d em=%zu ref=%zu\n", op.key, em_r, ref_r);
            if (em_r != ref_r) {
                fprintf(stderr, "  MISMATCH!\n");
                abort();
            }
            break;
        }
        case OP_ACCESS: {
            em.insert(op.key);
            ref.insert(op.key);
            fprintf(stderr, "  ACCESS key=%d\n", op.key);
            break;
        }
        case OP_INSERT_OR_ASSIGN: {
            em.insert(op.key);
            ref.insert(op.key);
            fprintf(stderr, "  INSERT_OR_ASSIGN key=%d\n", op.key);
            break;
        }
        case OP_FIND: {
            auto em_it = em.find(op.key);
            auto ref_it = ref.find(op.key);
            fprintf(stderr, "  FIND key=%d em=%d ref=%d\n", op.key, em_it != em.end(), ref_it != ref.end());
            break;
        }
        case OP_ITERATE: {
            std::unordered_set<int> em_collect;
            for (auto it = em.begin(); it != em.end(); ++it)
                em_collect.insert(*it);
            fprintf(stderr, "  ITERATE count=%zu\n", em_collect.size());
            break;
        }
        case OP_CLEAR: {
            em.clear();
            ref.clear();
            fprintf(stderr, "  CLEAR\n");
            break;
        }
        case OP_RESERVE: {
            size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
            if (cap < 1000000)
                (void)em.reserve(cap);
            fprintf(stderr, "  RESERVE cap=%zu\n", cap);
            break;
        }
        case OP_ERASE_ITERATOR: {
            auto em_it = em.find(op.key);
            auto ref_it = ref.find(op.key);
            if (em_it != em.end() && ref_it != ref.end()) {
                (void)em.erase(em_it);
                ref.erase(ref_it);
                fprintf(stderr, "  ERASE_ITER key=%d done\n", op.key);
            } else {
                fprintf(stderr, "  ERASE_ITER key=%d skip em=%d ref=%d\n", op.key, em_it != em.end(),
                        ref_it != ref.end());
            }
            break;
        }
        case OP_COUNT: {
            fprintf(stderr, "  COUNT key=%d em=%zu ref=%zu\n", op.key, (size_t)em.count(op.key), ref.count(op.key));
            break;
        }
        case OP_CONTAINS: {
            fprintf(stderr, "  CONTAINS key=%d em=%d ref=%d\n", op.key, em.contains(op.key), ref.count(op.key) > 0);
            break;
        }
        case OP_EMPLACE: {
            auto em_r = em.emplace(op.key);
            auto ref_r = ref.emplace(op.key);
            fprintf(stderr, "  EMPLACE key=%d em_ok=%d ref_ok=%d\n", op.key, em_r.second, ref_r.second);
            break;
        }
        }

        if (em.size() != ref.size()) {
            fprintf(stderr, "SIZE MISMATCH! em=%u ref=%zu after op[%zu]\n", (unsigned)em.size(), ref.size(), i);
            abort();
        }
    }

    fprintf(stderr, "Final verification...\n");
    for (const auto& k : ref) {
        if (!em.contains(k)) {
            fprintf(stderr, "LOST KEY: %d exists in ref but not in em!\n", k);
            abort();
        }
    }
    fprintf(stderr, "Done\n");
    return 0;
}
