//
// Created by schrodinger on 12/16/20.
//

#ifndef AHASH_RANDOM_STATE_H
#define AHASH_RANDOM_STATE_H

#include <stdint.h>

typedef struct random_state_s
{
  uint64_t keys[4];
} random_state_t;

random_state_t new_state();
void reinitialize_global_seed(uint64_t a, uint64_t b, uint64_t c, uint64_t d);

#define CREATE_HASHER(state) \
  hasher_from_random_state( \
    state.keys[0], state.keys[1], state.keys[2], state.keys[3])
#endif // AHASH_RANDOM_STATE_H
