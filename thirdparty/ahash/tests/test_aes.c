//
// Created by schrodinger on 12/16/20.
//
#include <ahash.h>
#include <assert.h>
#include <definitions.h>
#include <random_state.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ASSERT(x) \
  do \
  { \
    if (!(x)) \
    { \
      fflush(stdout); \
      abort(); \
    } \
  } while (0);

void shuffle_no_collide_with_aes()
{
#ifndef AHASH_USE_FALLBACK
  uint8_t value[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  aes128_t zero_mask_encode = aes_encode((aes128_t){}, (aes128_t){});
  aes128_t zero_mask_decode = aes_decode((aes128_t){}, (aes128_t){});
  for (int i = 0; i < 16; ++i)
  {
    value[i] = 1;
    aes128_t encode = aes_encode(*(aes128_t*)(value), zero_mask_encode);
    aes128_t decode = aes_decode(*(aes128_t*)(value), zero_mask_decode);
    aes128_t shuffled = shuffle(*(aes128_t*)(value));
    uint8_t* encode_vec = (uint8_t*)&encode;
    uint8_t* decode_vec = (uint8_t*)&decode;
    uint8_t* shuffled_vec = (uint8_t*)&shuffled;
    for (int j = 0; j < 16; ++j)
    {
      printf("val[%d]=%d, ", j, value[j]);
      printf("vec[%d]=%d, ", j, shuffled_vec[j]);
      printf("enc[%d]=%d, ", j, encode_vec[j]);
      printf("dec[%d]=%d\n", j, decode_vec[j]);
      if (shuffled_vec[j] != 0)
      {
        ASSERT(encode_vec[j] == 0);
        ASSERT(decode_vec[j] == 0);
      }
    }
    printf("\n");
    value[i] = 0;
  }
#endif
}

void unique()
{
  random_state_t a = new_state(), b = new_state();
  uint64_t ra = finish(CREATE_HASHER(a)), rb = finish(CREATE_HASHER(b));
  printf("unique test: ra=%lu, rb=%lu\n", ra, rb);
  ASSERT(ra != rb);
}

void same()
{
  random_state_t a = new_state();
  ahasher_t x = CREATE_HASHER(a), y = CREATE_HASHER(a);
  uint64_t address = (ptrdiff_t)same;
  x = write_uint64_t(x, address);
  y = write_uint64_t(y, address);
  uint64_t rx = finish(x), ry = finish(y);
  printf("same test: rx=%lu, ry=%lu\n", rx, ry);
  ASSERT(rx == ry);
}

char* generate_random_strings(size_t count, size_t length)
{
  char* string = (char*)malloc(length * count);
  for (size_t i = 0; i < count; ++i)
  {
    for (size_t j = 0; j < length; ++j)
    {
      string[i * length + j] = (char)rand() ^ (char)(time(0));
    }
  }
  return string;
}

void hash_string_bench()
{
  clock_t start, end;
  double cpu_time_used;
  char* tasks = generate_random_strings(10000, 1000);
  start = clock();
  random_state_t state = new_state();
  ahasher_t hasher = CREATE_HASHER(state);
  printf("start perf\n");
  uint64_t res = 0;
  for (size_t i = 0; i < 10000; ++i)
  {
    ahasher_t temp = hash_write(hasher, tasks + i * 1000, 1000);
    res += finish(temp);
  }
  end = clock();
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC * 1e9 / 10000.0;
  printf("esp time: %lf ns/1000bytes, res: %lu\n", cpu_time_used, res);
  free(tasks);
}

void random_equal()
{
  printf("testing byte using write\n");
  for (uint8_t i = 0; i < 255; ++i)
  {
    ASSERT(ahash64(&i, sizeof(uint8_t), i) == ahash64(&i, sizeof(uint8_t), i));
  }
  printf("testing int using write\n");
  for (int i = 0; i < 65536; ++i)
  {
    ASSERT(ahash64(&i, sizeof(int), i) == ahash64(&i, sizeof(int), i));
  }
  printf("testing different states\n");
  for (int i = 0; i < 65536; ++i)
  {
    random_state_t state = new_state();
    ahasher_t hasher1 = CREATE_HASHER(state);
    ahasher_t hasher2 = CREATE_HASHER(state);
    ASSERT(
      finish(write_uint64_t(hasher1, i)) == finish(write_uint64_t(hasher2, i)));
  }
  printf("testing strings\n");
  for (uint64_t i = 0; i < 233; ++i)
  {
    uint64_t seed = i ^ (ptrdiff_t)&random_equal;
    char* data = generate_random_strings(1, 5261);
    ASSERT(ahash64(data, 5261, seed) == ahash64(data, 5261, seed));
    free(data);
  }
}

int main()
{
  printf("%s\n", ahash_version());
  shuffle_no_collide_with_aes();
  unique();
  same();
  random_equal();
  hash_string_bench();
}
