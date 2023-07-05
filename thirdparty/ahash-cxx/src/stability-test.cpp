#include <ahash-cxx/hasher.h>
#include <vector>
#include <random>
#include <iostream>

struct OffsetArray {
    size_t offset;
    size_t length;
    uint8_t *base;

    OffsetArray(size_t offset, size_t length) : offset(offset), length(length) {
        base = static_cast<uint8_t *>(::operator new(length + offset, std::align_val_t{2048}));
        base = base + offset;
    }

    ~OffsetArray() {
        ::operator delete(base - offset, std::align_val_t{2048});
    }
};

template<typename Hasher>
void test_continuous() {
    std::random_device dev{};
    std::default_random_engine eng{dev()};
    std::uniform_int_distribution<uint8_t> dist{};
    for (size_t length = 1; length <= 32768; length *= 2) {
        std::vector<uint8_t> data(length);
        for (auto &i: data) {
            i = dist(eng);
        }
        Hasher hasher{length};
        hasher.consume(data.data(), data.size());
        uint64_t expected = hasher.finalize();
        for (size_t offset = 0; offset <= 64; ++offset) {
            auto array = OffsetArray{offset, length};
            std::memcpy(array.base, data.data(), data.size());
            Hasher inner{length};
            inner.consume(array.base, array.length);
            uint64_t real = inner.finalize();
            if (expected != real) {
                std::cout << "expected: " << expected << ", real: " << real << std::endl;
                ::abort();
            }
        }
    }
}

template<typename Hasher>
void test_multiple() {
    std::random_device dev{};
    std::default_random_engine eng{dev()};
    std::uniform_int_distribution<uint8_t> dist{};

    for (size_t length = 1; length <= 32768; length *= 2) {
        std::vector<size_t> steps;
        size_t total = 0;
        std::uniform_int_distribution<size_t> len_dist{};
        while (total != length && steps.size() < 64) {
            auto l = len_dist(eng) % (length - total);
            steps.push_back(l);
            total += l;
        }
        if (total != length) {
            steps.push_back(length - total);
        }
        auto hash = [&steps, length](uint8_t *base) {
            Hasher hasher{length};
            for (auto i: steps) {
                hasher.consume(base, i);
                base += i;
            }
            return hasher.finalize();
        };
        std::vector<uint8_t> data(length);
        for (auto &i: data) {
            i = dist(eng);
        }
        uint64_t expected = hash(data.data());
        for (size_t offset = 0; offset <= 64; ++offset) {
            auto array = OffsetArray{offset, length};
            std::memcpy(array.base, data.data(), data.size());
            uint64_t real = hash(array.base);
            if (expected != real) {
                std::cout << "expected: " << expected << ", real: " << real << std::endl;
                ::abort();
            }
        }
    }
}

int main() {
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
    test_continuous<ahash::VectorizedHasher>();
    test_multiple<ahash::VectorizedHasher>();
#endif
    test_continuous<ahash::FallbackHasher<ahash::DEFAULT_MULTIPLE, ahash::DEFAULT_ROT>>();
    test_multiple<ahash::FallbackHasher<ahash::DEFAULT_MULTIPLE, ahash::DEFAULT_ROT>>();
}