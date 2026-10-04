#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

// Include all emilib hash map headers (paths relative to bench/emilib_bench/)
#include "../../include/emilib/emihmap1.hpp"
#include "../../include/emilib/emihmap2.hpp"
#include "../../include/emilib/emihmap3.hpp"
#include "../../include/emilib/emihmap4.hpp"
#include "../../include/emilib/emihmap2_v2.hpp"

// Include benchmark framework headers
#include "bench_hash.hpp"
#include "bench_runner.hpp"

namespace emilib_bench {

// ============================================================================
// MapInfo: describes a registered map type for benchmarking
// ============================================================================
template <typename KeyT, typename ValueT>
struct MapInfo {
    std::string name;
    std::string hash_name;
    // Factory function: creates a new map instance (type-erased via void*)
    std::function<void*()> factory;
    // Destroy function: deletes a map instance
    std::function<void(void*)> destroyer;
    // Type-erased benchmark runner
    std::function<std::vector<BenchResult>()> run_all_fn;
};

// ============================================================================
// MapFactory: primary template for creating HashMap instances
// ============================================================================
template <typename MapT, typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>>
struct MapFactory {
    using MapType = MapT;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() {
        return "HashMap";
    }
};

// ============================================================================
// Specializations for each emilib version
// ============================================================================

// emilib::HashMap (emihmap1)
template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<emilib::HashMap<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = emilib::HashMap<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "emilib::HashMap"; }
};

// emilib2::HashMap (emihmap2)
template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<emilib2::HashMap<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = emilib2::HashMap<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "emilib2::HashMap"; }
};

// emilib3::HashMap (emihmap3)
template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<emilib3::HashMap<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = emilib3::HashMap<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "emilib3::HashMap"; }
};

// emilib4::HashMap (emihmap4)
template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<emilib4::HashMap<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = emilib4::HashMap<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "emilib4::HashMap"; }
};

// emilib2_v2::HashMap (emihmap2_v2)
template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<emilib2_v2::HashMap<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = emilib2_v2::HashMap<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "emilib2_v2::HashMap"; }
};

// ============================================================================
// Boost unordered_flat_map registration
// ============================================================================
#ifdef EMILIB_BENCH_ENABLE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>

template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<boost::unordered_flat_map<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = boost::unordered_flat_map<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "boost::unordered_flat_map"; }
};
#endif // EMILIB_BENCH_ENABLE_BOOST

// ============================================================================
// Abseil flat_hash_map registration
// ============================================================================
#ifdef EMILIB_BENCH_ENABLE_ABSL
#include <absl/container/flat_hash_map.h>

template <typename KeyT, typename ValueT, typename HashT>
struct MapFactory<absl::flat_hash_map<KeyT, ValueT, HashT>, KeyT, ValueT, HashT> {
    using MapType = absl::flat_hash_map<KeyT, ValueT, HashT>;

    static MapType create() {
        return MapType();
    }

    static MapType create_with_hash(const HashT& hasher = HashT()) {
        return MapType(0, hasher);
    }

    static std::string name() { return "absl::flat_hash_map"; }
};
#endif // EMILIB_BENCH_ENABLE_ABSL

// ============================================================================
// Map registry: get all registered maps for a given KeyT/ValueT pair
// ============================================================================

// Lightweight registry entry that doesn't require type erasure of the map itself,
// only stores metadata and a factory lambda for creating BenchRunner instances.
template <typename KeyT, typename ValueT = int>
struct RegisteredMap {
    std::string name;
    std::string hash_name;
    // Lambda that, when called with a BenchConfig, returns all BenchResults
    std::function<std::vector<BenchResult>(const BenchConfig&)> run_with_config;
};

// Helper to register a map type with std::hash
template <typename MapT, typename KeyT, typename ValueT = int>
RegisteredMap<KeyT, ValueT> make_registered_map(const std::string& name) {
    return RegisteredMap<KeyT, ValueT>{
        name,
        "std::hash",
        [name](const BenchConfig& config) -> std::vector<BenchResult> {
            BenchRunner<MapT, KeyT, ValueT> runner(name, "std::hash", config);
            return runner.run_all();
        }
    };
}

// Get all registered maps with std::hash for a given KeyT/ValueT
template <typename KeyT, typename ValueT = int>
std::vector<RegisteredMap<KeyT, ValueT>> get_registered_maps() {
    std::vector<RegisteredMap<KeyT, ValueT>> maps;

    // emilib versions (all use std::hash by default)
    maps.push_back(make_registered_map<emilib::HashMap<KeyT, ValueT>, KeyT, ValueT>("emilib1::HashMap"));
    maps.push_back(make_registered_map<emilib2::HashMap<KeyT, ValueT>, KeyT, ValueT>("emilib2::HashMap"));
    maps.push_back(make_registered_map<emilib3::HashMap<KeyT, ValueT>, KeyT, ValueT>("emilib3::HashMap"));
    maps.push_back(make_registered_map<emilib4::HashMap<KeyT, ValueT>, KeyT, ValueT>("emilib4::HashMap"));
    maps.push_back(make_registered_map<emilib2_v2::HashMap<KeyT, ValueT>, KeyT, ValueT>("emilib2_v2::HashMap"));

#ifdef EMILIB_BENCH_ENABLE_BOOST
    maps.push_back(make_registered_map<boost::unordered_flat_map<KeyT, ValueT>, KeyT, ValueT>("boost::unordered_flat_map"));
#endif

#ifdef EMILIB_BENCH_ENABLE_ABSL
    maps.push_back(make_registered_map<absl::flat_hash_map<KeyT, ValueT>, KeyT, ValueT>("absl::flat_hash_map"));
#endif

    return maps;
}

// Get registered maps with alternative hash functions (wyhash, FNV-1a, mul-xor)
template <typename KeyT, typename ValueT = int>
std::vector<RegisteredMap<KeyT, ValueT>> get_registered_maps_all_hashes() {
    std::vector<RegisteredMap<KeyT, ValueT>> maps;

    // std::hash variants
    auto std_maps = get_registered_maps<KeyT, ValueT>();
    for (auto& m : std_maps) {
        maps.push_back(std::move(m));
    }

    // Wyhash variants for emilib maps
    if constexpr (std::is_integral_v<KeyT>) {
        using WH = WyHashInt<KeyT>;
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib1::HashMap", "emh_wyhash",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib::HashMap<KeyT, ValueT, WH>, KeyT, ValueT> runner("emilib1::HashMap", "emh_wyhash", config);
                return runner.run_all();
            }
        });
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib4::HashMap", "emh_wyhash",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib4::HashMap<KeyT, ValueT, WH>, KeyT, ValueT> runner("emilib4::HashMap", "emh_wyhash", config);
                return runner.run_all();
            }
        });
    } else if constexpr (std::is_same_v<KeyT, std::string>) {
        using WH = WyHashString;
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib1::HashMap", "emh_wyhash",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib::HashMap<KeyT, ValueT, WH>, KeyT, ValueT> runner("emilib1::HashMap", "emh_wyhash", config);
                return runner.run_all();
            }
        });
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib4::HashMap", "emh_wyhash",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib4::HashMap<KeyT, ValueT, WH>, KeyT, ValueT> runner("emilib4::HashMap", "emh_wyhash", config);
                return runner.run_all();
            }
        });
    }

    // FNV-1a variants
    if constexpr (std::is_integral_v<KeyT>) {
        using FH = Fnv1aIntHash<KeyT>;
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib4::HashMap", "FNV-1a",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib4::HashMap<KeyT, ValueT, FH>, KeyT, ValueT> runner("emilib4::HashMap", "FNV-1a", config);
                return runner.run_all();
            }
        });
    } else if constexpr (std::is_same_v<KeyT, std::string>) {
        using FH = Fnv1aHash;
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib4::HashMap", "FNV-1a",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib4::HashMap<KeyT, ValueT, FH>, KeyT, ValueT> runner("emilib4::HashMap", "FNV-1a", config);
                return runner.run_all();
            }
        });
    }

    // Mul-xor variants (integers only)
    if constexpr (std::is_integral_v<KeyT>) {
        using MX = MulXorHash<KeyT>;
        maps.push_back(RegisteredMap<KeyT, ValueT>{
            "emilib4::HashMap", "mul-xor",
            [](const BenchConfig& config) -> std::vector<BenchResult> {
                BenchRunner<emilib4::HashMap<KeyT, ValueT, MX>, KeyT, ValueT> runner("emilib4::HashMap", "mul-xor", config);
                return runner.run_all();
            }
        });
    }

    return maps;
}

} // namespace emilib_bench
