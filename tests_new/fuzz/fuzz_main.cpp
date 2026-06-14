// Fuzz main wrapper for non-libfuzzer builds (gcc + ASan compatible)
// This file provides a standalone main() that drives LLVMFuzzerTestOneInput
// with random data, useful for CI testing without requiring clang+libfuzzer.
//
// Usage: ./fuzz_extreme [iterations]   (default 1000)

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static std::vector<uint8_t> gen_random_input(std::mt19937& rng, size_t len)
{
    std::vector<uint8_t> buf(len);
    for (auto& b : buf) b = (uint8_t)(rng() & 0xFF);
    return buf;
}

int main(int argc, char** argv)
{
    int iterations = 1000;
    if (argc > 1) iterations = std::atoi(argv[1]);

    std::random_device rd;
    std::mt19937 rng(rd());

    for (int i = 0; i < iterations; ++i) {
        size_t len = 10 + (rng() % 200);
        auto buf = gen_random_input(rng, len);
        LLVMFuzzerTestOneInput(buf.data(), buf.size());
    }

    return 0;
}
