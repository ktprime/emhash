#ifndef AHASH_AHASH_H
#define AHASH_AHASH_H 1
#include "definitions.h"

#ifndef AHASH_USE_FALLBACK
typedef struct ahasher_s
{
  aes128_t enc;
  aes128_t sum;
  aes128_t key;
} ahasher_t;
#else
#  include <string.h>
#  define AHASH_MULTIPLIER 6364136223846793005ull
#  define AHASH_ROT 23
typedef struct ahasher_s
{
  uint64_t buffer;
  uint64_t pad;
  uint64_t extra_keys[2];
} ahasher_t;
#endif

ahasher_t
hasher_from_random_state(uint64_t k0, uint64_t k1, uint64_t k2, uint64_t k3);

#define WRITE_API(TYPE) ahasher_t write_##TYPE(ahasher_t hasher, TYPE value)

WRITE_API(uint64_t);
WRITE_API(int64_t);
WRITE_API(uint32_t);
WRITE_API(int32_t);
WRITE_API(uint16_t);
WRITE_API(int16_t);
WRITE_API(uint8_t);
WRITE_API(int8_t);

ahasher_t add_length(ahasher_t hasher, size_t length);
ahasher_t hash_write(ahasher_t hasher, const char* input, size_t size);
uint64_t finish(ahasher_t hasher);
uint64_t ahash64(const void*  buf, size_t size, uint64_t seed);
const char* ahash_version(void);
#endif // AHASH_AHASH_H
