// standalone microbenchmark mirroring bench_quick_overall_udm workloads
// Extracted from: 2026-07-08_1030-map-performance-optimization.md (Appendix A)
// Build:
//   clang++ -O3 -DNDEBUG -std=c++17 -I<repo>/include -I<repo>/test micro.cpp nanobench_impl.cpp -o micro

#include <ankerl/unordered_dense.h>
#include <third-party/nanobench.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

template <typename K> inline auto init_key() -> K {
    return {};
}

template <typename T> inline void randomize_key(ankerl::nanobench::Rng* rng, int n, T* key) {
    auto limited = (((*rng)() >> 32U) * static_cast<uint64_t>(n)) >> 32U;
    *key = static_cast<T>(limited);
}

template <> [[nodiscard]] inline auto init_key<std::string>() -> std::string {
    std::string str;
    str.resize(200);
    return str;
}

inline void randomize_key(ankerl::nanobench::Rng* rng, int n, std::string* key) {
    uint64_t k{};
    randomize_key(rng, n, &k);
    std::memcpy(key->data(), &k, sizeof(k));
}

template <typename Map> uint64_t insert_erase() {
    ankerl::nanobench::Rng rng(123);
    size_t verifier{};
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (int n = 1; n < 20000; ++n) {
        for (int i = 0; i < 200; ++i) {
            randomize_key(&rng, n, &key);
            map[key];
            randomize_key(&rng, n, &key);
            verifier += map.erase(key);
        }
    }
    return verifier + map.size(); // expect 1994641 + 9987
}

template <typename Map> uint64_t find_50() {
    uint64_t const seed = 123123;
    ankerl::nanobench::Rng numbers_insert_rng(seed);
    size_t numbers_insert_rng_calls = 0;
    ankerl::nanobench::Rng numbers_search_rng(seed);
    size_t numbers_search_rng_calls = 0;
    ankerl::nanobench::Rng insertion_rng(123);
    size_t checksum = 0;
    Map map;
    auto key = init_key<typename Map::key_type>();
    for (size_t i = 0; i < 100000; ++i) {
        randomize_key(&numbers_insert_rng, 1000000, &key);
        ++numbers_insert_rng_calls;
        if (insertion_rng() & 1U) {
            map[key] = i;
        }
        for (size_t search = 0; search < 100; ++search) {
            randomize_key(&numbers_search_rng, 1000000, &key);
            ++numbers_search_rng_calls;
            auto it = map.find(key);
            if (it != map.end()) {
                checksum += it->second;
            }
            if (numbers_insert_rng_calls == numbers_search_rng_calls) {
                numbers_search_rng = ankerl::nanobench::Rng(seed);
                numbers_search_rng_calls = 0;
            }
        }
    }
    return checksum;
}

template <typename Map> uint64_t iterate() {
    size_t const num_elements = 5000;
    auto key = init_key<typename Map::key_type>();
    ankerl::nanobench::Rng rng(555);
    Map map;
    size_t result = 0;
    for (size_t n = 0; n < num_elements; ++n) {
        randomize_key(&rng, 1000000, &key);
        map[key] = n;
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    }
    rng = ankerl::nanobench::Rng(555);
    do {
        randomize_key(&rng, 1000000, &key);
        map.erase(key);
        for (auto const& key_val : map) {
            result += key_val.second;
        }
    } while (!map.empty());
    return result; // expect 62282755409
}

using map_u64 = ankerl::unordered_dense::map<uint64_t, size_t>;
using map_str = ankerl::unordered_dense::map<std::string, size_t, ankerl::unordered_dense::hash<std::string>>;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: %s [ie64|iestr|find64|findstr|it64|itstr] [reps]\n", argv[0]);
        return 1;
    }
    int reps = argc > 2 ? atoi(argv[2]) : 1;
    uint64_t sink = 0;
    double best = 1e30;
    for (int r = 0; r < reps; ++r) {
        auto t0 = chrono::steady_clock::now();
        if (0 == strcmp(argv[1], "ie64")) {
            sink += insert_erase<map_u64>();
        } else if (0 == strcmp(argv[1], "iestr")) {
            sink += insert_erase<map_str>();
        } else if (0 == strcmp(argv[1], "find64")) {
            sink += find_50<map_u64>();
        } else if (0 == strcmp(argv[1], "findstr")) {
            sink += find_50<map_str>();
        } else if (0 == strcmp(argv[1], "it64")) {
            sink += iterate<map_u64>();
        } else if (0 == strcmp(argv[1], "itstr")) {
            sink += iterate<map_str>();
        }
        auto t1 = chrono::steady_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
        if (ms < best) {
            best = ms;
        }
    }
    (void)sink;
    printf("%.1f\n", best);
    return 0;
}