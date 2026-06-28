// common/utilities.hpp
// Shared helpers: make_kv (int->key/value conversion), now_ms (timing),
// oracle comparison helpers.
// Extracted from the per-file definitions previously duplicated in
// verify/test_all_maps.cpp:54, attack/hash_attack_all.cpp:61, etc.
#pragma once

#include <chrono>
#include <cstdio>
#include <string>
#include <type_traits>
#include <unordered_map>

// Convert an int index into a key or value of type T.
// std::string -> std::to_string(v); numeric types -> T(v).
template <typename T>
inline T make_kv(int v) {
    using U = std::remove_cv_t<T>;
    if constexpr (std::is_same_v<U, std::string>) {
        return std::to_string(v);
    } else if constexpr (std::is_same_v<U, const char*>) {
        // not owned; caller must ensure lifetime
        static thread_local char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", v);
        return buf;
    } else {
        return static_cast<T>(v);
    }
}

// Millisecond timestamp for performance baselines in attack tests.
inline long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Compare a map under test against a std::unordered_map oracle.
// Returns true iff sizes match and every oracle key is found with equal value.
template <typename MapT, typename K, typename V>
inline bool oracle_equal(const MapT& m, const std::unordered_map<K, V>& ref) {
    if (m.size() != ref.size()) return false;
    for (const auto& kv : ref) {
        auto it = m.find(kv.first);
        if (it == m.end()) return false;
        if (!(it->second == kv.second)) return false;
    }
    return true;
}
