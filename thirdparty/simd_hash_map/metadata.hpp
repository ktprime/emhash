//Copyright Nathan Ward 2019.

#ifndef METADATA_HPP
#define METADATA_HPP

using metadata = signed char;

enum md_states : metadata {
  mdEmpty = -128,  // 0b10000000
};

static inline bool isFull(const metadata md) { return md >= 0; }
static inline bool isEmpty(const metadata md) {
  return md == md_states::mdEmpty;
}

#endif  // METADATA_HPP
