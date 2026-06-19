// Debug emhash8 chain corruption with FuzzHasher
// Compile: clang++-20 -fsanitize=address -std=c++17 -g -O0 -I. fuzz/debug_chain.cpp -o fuzz/debug_chain
#include "emhash/hash_table8.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <fstream>

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
        printf("Usage: %s <crash_file>\n", argv[0]);
        return 1;
    }

    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    f.close();

    if (data.empty()) {
        printf("Empty file\n");
        return 1;
    }

    uint8_t test_selector = data[0] % 10;
    const uint8_t* op_data = data.data() + 1;
    size_t op_size = data.size() - 1;

    printf("test_selector=%d data_size=%zu\n", test_selector, data.size());

    auto ops = parse_input(op_data, op_size);
    if (ops.size() > 500)
        ops.resize(500);
    printf("num_ops=%zu\n", ops.size());

    if (test_selector == 8) { // FuzzHasher collision test
        emhash8::HashMap<int, int, FuzzHasher> em(2, 0.8f);
        std::unordered_map<int, int> ref;

        for (size_t i = 0; i < ops.size(); i++) {
            const auto& op = ops[i];
            fprintf(stderr, "[%zu] op=%d key=%d val=%d | em.size=%u ref.size=%zu\n", i, (int)op.code, op.key, op.value,
                    (unsigned)em.size(), ref.size());

            switch (op.code) {
            case OP_INSERT: {
                auto em_r = em.insert({op.key, op.value});
                auto ref_r = ref.insert({op.key, op.value});
                fprintf(stderr, "  INSERT key=%d em_ok=%d ref_ok=%d\n", op.key, em_r.second, ref_r.second);
                if (em_r.second != ref_r.second) {
                    fprintf(stderr, "  MISMATCH! em_ok=%d ref_ok=%d for key=%d\n", em_r.second, ref_r.second, op.key);
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
                em[op.key] = op.value;
                ref[op.key] = op.value;
                fprintf(stderr, "  ACCESS key=%d val=%d\n", op.key, op.value);
                break;
            }
            case OP_ERASE_ITERATOR: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                if (em_it != em.end() && ref_it != ref.end()) {
                    em.erase(em_it);
                    ref.erase(ref_it);
                    printf("  ERASE_ITER done\n");
                } else {
                    printf("  ERASE_ITER skip em=%d ref=%d\n", em_it != em.end(), ref_it != ref.end());
                }
                break;
            }
            case OP_FIND: {
                auto em_it = em.find(op.key);
                auto ref_it = ref.find(op.key);
                printf("  FIND em=%d ref=%d\n", em_it != em.end(), ref_it != ref.end());
                break;
            }
            case OP_ITERATE: {
                std::unordered_map<int, int> em_collect;
                for (auto it = em.begin(); it != em.end(); ++it)
                    em_collect[it->first] = it->second;
                printf("  ITERATE count=%zu\n", em_collect.size());
                break;
            }
            case OP_CLEAR: {
                em.clear();
                ref.clear();
                break;
            }
            case OP_RESERVE: {
                size_t cap = static_cast<size_t>(op.key & 0x7FFFFFFF);
                if (cap < 1000000)
                    em.reserve(cap);
                break;
            }
            case OP_INSERT_OR_ASSIGN: {
                int v = op.value;
                em.insert_or_assign(op.key, std::move(v));
                ref[op.key] = op.value;
                break;
            }
            case OP_COUNT: {
                printf("  COUNT em=%zu ref=%zu\n", (size_t)em.count(op.key), ref.count(op.key));
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
            }

            if (em.size() != ref.size()) {
                printf("SIZE MISMATCH! em=%zu ref=%zu after op[%zu]\n", (size_t)em.size(), ref.size(), i);
                abort();
            }
        }
    } else {
        printf("Not a FuzzHasher test (selector=%d), skipping\n", test_selector);
    }

    printf("Done\n");
    return 0;
}
