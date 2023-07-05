#include <ahash-cxx/hasher.h>
#include <cstdint>
#include <limits>
#include <cstdlib>
#include <iostream>

using namespace ahash;

int main() {
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
    union {
        unsigned __int128 data = std::numeric_limits<uint64_t>::max();
        VectorOperator::VecType vector;
    };
    data <<= 64;
    data |= 50;
    vector = VectorOperator::add_extra_data(vector, std::numeric_limits<uint64_t>::max());
    uint64_t low = data & std::numeric_limits<uint64_t>::max();
    if (low != 49) {
        std::cout << std::hex << "low: " << low << std::endl;
        ::abort();
    }
    uint64_t high = (data >> 64);
    if (high != std::numeric_limits<uint64_t>::max()) {
        std::cout << std::hex << "high: " << high << std::endl;
        ::abort();
    }
#endif
}