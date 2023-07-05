#pragma once
namespace ahash {
    __attribute__((visibility("default"), used)) uint64_t hash(const void *buf, size_t len, uint64_t seed);
    __attribute__((visibility("default"), used)) uint64_t fallback_hash(const void *buf, size_t len, uint64_t seed);
    __attribute__((visibility("default"), used, const)) bool has_basic_simd();
    __attribute__((visibility("default"), used, const)) bool has_wide_simd();
} // namespace ahash
