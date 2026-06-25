#include <cstdint>
using size_type = uint32_t;
static constexpr size_type INACTIVE = 0 - 0x1u;

struct entry {
    int second;
    size_type bucket;
    int first;
};

entry* _pairs;

bool check_empty(size_type bucket) {
    auto next_bucket = _pairs[bucket].bucket;
    return static_cast<size_type>(next_bucket) < 0;
}

bool check_empty_int(size_type bucket) {
    auto next_bucket = _pairs[bucket].bucket;
    return static_cast<int>(next_bucket) < 0;
}
