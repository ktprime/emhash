#include <cstdint>
#include <cstdio>
#include <cstring>

using size_type = uint32_t;
static constexpr size_type INACTIVE = 0 - 0x1u;

struct entry {
    int second;
    size_type bucket;
    int first;
};

int main() {
    entry e;
    e.bucket = INACTIVE;
    e.first = 0;
    e.second = 0;

    // Simulate the check in find_or_kickout
    auto next_bucket = e.bucket;
    printf("next_bucket = 0x%x\n", next_bucket);
    printf("static_cast<size_type>(next_bucket) < 0 = %d\n", (int)(static_cast<size_type>(next_bucket) < 0));
    printf("static_cast<int>(next_bucket) < 0 = %d\n", (int)(static_cast<int>(next_bucket) < 0));
    printf("next_bucket == INACTIVE = %d\n", (int)(next_bucket == INACTIVE));

    // Check with -O2 optimization (like the hash map is compiled)
    volatile size_type vb = INACTIVE;
    printf("\nWith volatile (forces read):\n");
    printf("static_cast<size_type>(vb) < 0 = %d\n", (int)(static_cast<size_type>(vb) < 0));
    return 0;
}
