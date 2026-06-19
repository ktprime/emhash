// Minimal fuzzer for the crash sequence - using gcc to compile
#include <cstdint>
#include <cstdio>
#include <vector>
#include <fstream>
#include <cassert>
#include "emhash/hash_table8.hpp"

int main() {
    std::ifstream f("../fuzz/crash-1909d956d6d9d6a7fa8a4145a9532469e499d0cc", std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    emhash8::HashMap<int, int> em;

    auto op = [&](size_t i) {
        uint8_t code = data[i] % 10;
        int32_t key = *reinterpret_cast<int32_t*>(&data[i + 1]);
        int32_t val = *reinterpret_cast<int32_t*>(&data[i + 5]);
        switch (code) {
        case 0:
            em.insert({key, val});
            break;
        case 1:
            em.erase(key);
            break;
        case 2:
            em.find(key);
            break;
        case 3:
            em[key] = val;
            break;
        case 4: {
            for (auto it = em.begin(); it != em.end(); ++it)
                (void)it->first;
        } break;
        case 5:
            em.clear();
            break;
        case 6: {
            size_t cap = static_cast<size_t>(key & 0x7FFFFFFF);
            if (cap < 1000000)
                em.reserve(cap);
        } break;
        case 7: {
            int v = val;
            em.insert_or_assign(key, std::move(v));
        } break;
        case 8: {
            auto it = em.find(key);
            if (it != em.end())
                em.erase(it);
        } break;
        case 9:
            em.count(key);
            break;
        }
    };

    printf("Replaying %zu operations\n", data.size() / 9);
    for (size_t i = 0; i + 9 <= data.size(); i += 9) {
        printf("Op %zu: code=%u key=0x%08X val=0x%08X\n", i / 9, data[i] % 10,
               *reinterpret_cast<int32_t*>(&data[i + 1]), *reinterpret_cast<int32_t*>(&data[i + 5]));
        op(i);
        printf("  -> size=%u\n", em.size());
    }
    printf("DONE - no crash\n");
    return 0;
}
