// common/hashers.hpp
// Bad hash functors used by attack / stress / fuzz tests.
// Extracted from the multiple per-file definitions that previously existed in
// attack/hash_attack_all.cpp, debug/debug_chain.cpp, fuzz/fuzz_extreme.cpp, etc.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// All keys map to bucket 0 -> worst-case collision chain
struct ConstHasher {
    std::size_t operator()(std::size_t) const { return 0; }
};

// Keys collapse into 4 buckets -> bounded but heavy collision
struct Range4Hasher {
    std::size_t operator()(std::size_t k) const { return k % 4; }
};

// Identity hash on size_t (still collides when keys share low bits)
struct LinearHasher {
    std::size_t operator()(std::size_t k) const { return k; }
};

// String hash based on length only -> collides for equal-length strings
struct BadStringHasher {
    std::size_t operator()(const std::string& s) const { return s.size() % 4; }
};

// Generic colliding hash templated on key type
template <typename K>
struct CollidingHash;

template <>
struct CollidingHash<int> {
    std::size_t operator()(int k) const { return static_cast<std::size_t>(k) % 4; }
};

template <>
struct CollidingHash<std::string> {
    std::size_t operator()(const std::string& s) const { return s.size() % 4; }
};

// Int-key colliding hasher usable as a template argument with int keys
struct IntMod4Hasher {
    std::size_t operator()(int k) const { return static_cast<std::size_t>(k) % 4; }
};
