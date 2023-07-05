#include <ahash-cxx/hasher.h>
#include <ahash-cxx/ahash-cxx.h>

namespace ahash {
    __attribute__((visibility("default"), used)) uint64_t
    hash(const void *buf, size_t len, uint64_t seed) {
        Hasher hasher{seed};
        hasher.consume(buf, len);
        return hasher.finalize();
    }

    __attribute__((visibility("default"), used)) uint64_t fallback_hash(const void *buf, size_t len, uint64_t seed) {
        FallbackHasher<DEFAULT_MULTIPLE, DEFAULT_ROT> hasher{seed};
        hasher.consume(buf, len);
        return hasher.finalize();
    }

    __attribute__((visibility("default"), used, const)) bool has_basic_simd() {
#if AHASH_CXX_HAS_BASIC_SIMD_ACCELERATION
        return true;
#else
        return false;
#endif
    }

    __attribute__((visibility("default"), used, const)) bool has_wide_simd() {
#if AHASH_CXX_HAS_WIDER_SIMD_ACCELERATION
        return true;
#else
        return false;
#endif
    }
}
