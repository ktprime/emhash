// Debug emilib2ss key=127 - direct internal state inspection
#include <cstdint>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <x86intrin.h>

#define private public
#define protected public
#include "emilib/emilib2ss.hpp"
#undef private
#undef protected

int main() {
    emilib::HashMap<int, int> em(4);

    FILE* f = fopen("crash-00d7b8c68c7e4555e93e83314dcbccbb849cf15f", "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);

    std::unordered_map<int, int> ref;

    size_t i = 0;
    int op_count = 0;
    while (i + 5 <= size) {
        uint8_t op_type = data[i] % 4;
        int key = (int)(data[i+1] | (data[i+2] << 8) | (data[i+3] << 16) | (data[i+4] << 24));
        i += 5;

        switch (op_type) {
            case 0: em[key] = key; ref[key] = key; break;
            case 1: { auto it = em.find(key); if (it != em.end()) em.erase(it); ref.erase(key); break; }
            case 2: break;
            case 3: em[key] = key; ref[key] = key; break;
        }
        op_count++;

        if (ref.count(127) && em.find(127) == em.end()) {
            printf("FAIL at op=%d type=%d key=%d\n", op_count, op_type, key);
            printf("em.size=%u bucket_count=%u\n", (unsigned)em.size(), (unsigned)em.bucket_count());

            // Find where key=127 actually is
            size_t found_bucket = (size_t)-1;
            for (size_t b = 0; b < em._num_buckets; b++) {
                if (b % 16 == 15) continue;
                if (em._states[b] >= emilib::State::EFILLED) {
                    if (em._pairs[b].first == 127) {
                        found_bucket = b;
                        break;
                    }
                }
            }

            // Compute main_bucket for key=127
            const auto key_hash = em._hasher(127);
            size_t main_bucket = size_t(key_hash & em._mask);
            main_bucket -= main_bucket % 16;

            printf("key=127: hash=0x%x main_bucket=%zu found_bucket=%zu\n",
                (unsigned)key_hash, main_bucket, found_bucket);

            // Compute h2 tag
            int8_t h2 = (int8_t)((size_t)(key_hash % 253) + (size_t)emilib::State::EFILLED);
            printf("h2 tag for key=127 = %d (0x%02x)\n", h2, (uint8_t)h2);

            // Print group_probe for main_bucket
            int gp = em.group_probe(main_bucket);
            printf("group_probe(main_bucket=%zu) = %d\n", main_bucket, gp);

            // Print states at main_bucket group
            printf("States at main_bucket group: ");
            for (int j = 0; j < 16; j++) {
                printf("%d ", (int)em._states[main_bucket + j]);
            }
            printf("\n");

            // Print states at found_bucket group
            if (found_bucket != (size_t)-1) {
                size_t fg = found_bucket / 16 * 16;
                printf("States at found_bucket group (%zu): ", fg);
                for (int j = 0; j < 16; j++) {
                    printf("%d ", (int)em._states[fg + j]);
                }
                printf("\n");
            }

            // Simulate find_filled_bucket
            printf("\nSimulating find_filled_bucket for key=127:\n");
            auto next_bucket = main_bucket;
            size_t offset = 0;
            const auto filled = _mm_set1_epi8(h2);
            int step = 0;
            while (step < 20) {
                const auto vec = _mm_loadu_si128((__m128i const*)&em._states[next_bucket]);
                auto maskf = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(vec, filled)) & 0x7FFF;
                printf("  offset=%zu next_bucket=%zu maskf=0x%04x\n",
                    offset, next_bucket, maskf);

                if (maskf) {
                    uint32_t m = maskf;
                    do {
                        auto fb = next_bucket + __builtin_ctz(m);
                        printf("    checking bucket=%zu key=%d\n", fb, em._pairs[fb].first);
                        if (em._pairs[fb].first == 127) {
                            printf("  FOUND at bucket %zu!\n", fb);
                            goto done;
                        }
                    } while (m &= m - 1);
                }

                if ((int)++offset > gp) {
                    printf("  STOP: offset=%d > group_probe=%d\n", (int)offset, gp);
                    break;
                }
                next_bucket = em.get_next_bucket(next_bucket, offset);
                step++;
            }
            done:
            break;
        }
    }

    return 0;
}
