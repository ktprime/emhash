#include <string>
#include <chrono>
#include <unordered_map>
#include <random>
#include <iostream>

//#define EMHASH_HIGH_LOAD 201000
//#define EMHASH_FIBONACCI_HASH  1

#include "sfc64.h"

#define EMHASH_HIGH_LOAD 201000

#include "tsl/robin_map.h"
#include "ska/flat_hash_map.hpp"
#include "martin/robin_hood.h"
#include "hash_table2.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table7.hpp"
#include "phmap/phmap.h"
#include "hrd/hash_set7.h"
#include "hash_table232.hpp"

#include "hash_set4.hpp"
#include "hash_set3.hpp"
using my_clock = std::chrono::high_resolution_clock;


template<typename Map>
void bench(char const* title)
{
    int trials = 100;
    size_t result = 0;

    Map map;

    map.max_load_factor(7.0/8);
    // Insert
    auto start = my_clock::now();
    {
        for (int i = 0; i < 10; ++i) {
            map.clear();
            for (int32_t i = 0; i < 1000000; i++)
            {
                map.emplace(i, "h");
            }
        }
    }
    result += map.size();
    double t_insert = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();

    // Iter
    start = my_clock::now();
    {
        for (size_t i = 0; i < trials; ++i) {
            for (auto const& keyVal : map)
            {
                result += keyVal.second.size();
            }
        }
    }
    double t_iter = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();

    // Find
    start = my_clock::now();
    {
        for (size_t x = 0; x < 10; ++x) {
            for (int32_t i = 0; i < 5000000; i++) {
                auto it = map.find(i);
                if (it != map.end())
                {
                    result += it->second.size();
                }
            }
        }
    }
    double t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
    printf("%6.2f insert %6.2f iter %6.2f find %lld result: %s|%.2f\n", t_insert, t_iter, t_find, result, title, map.load_factor());
}

int main()
{
    bench<robin_hood::unordered_node_map<int, std::string>>("robin_hood::unordered_node_map<int, std::string>");
    bench<std::unordered_map<int, std::string>>("std::unordered_map");
    bench<robin_hood::unordered_flat_map<int, std::string>>("robin_hood::unordered_flat_map");
    bench<emhash2::HashMap<int, std::string>>("emhash2::hashMap");
    bench<emhash5::HashMap<int, std::string>>("emhash5::hashMap");
    bench<tsl::robin_map<int, std::string>>("tsl::robin_map");
    bench<emhash7::HashMap<int, std::string>>("emhash7::hashMap");
    bench<phmap::flat_hash_map<int, std::string>>("phmap::hpmap");
    bench<ska::flat_hash_map<int, std::string>>("ska::flat_map");
    bench<emhash6::HashMap<int, std::string>>("emhash6::hashMap");
    bench<hrd7::hash_map<int, std::string>>("hrd7::hash_map");
    bench<emilib3::HashMap<int, std::string>>("emilib3::hashMap");

    srand(time(0));

    //phmap::flat_hash_map<int, std::string> mmap;
    robin_hood::unordered_node_map<int, std::string> mmap;
    //tsl::robin_map<int, std::string> mmap;
    //ska::flat_hash_map<int, std::string> mmap;

    for (int i = 0; i < 10000; i ++)
        mmap[rand()] = mmap[rand()];
    size_t maxSize = 1U << 28;
    size_t numReps = 100;

    std::random_device rd;
    auto rng = std::mt19937{rd()};
    auto dis = std::uniform_int_distribution<uint32_t>{0, (1U << 31) - 1};
    sfc64 srng(rd());

    for (size_t rep = 0; rep < numReps; ++rep) {
        auto start = my_clock::now();
#ifndef ET
        emhash9::HashSet<uint32_t> set;
#else
        ska::bytell_hash_set<uint32_t> set;
        //phmap::flat_hash_set<uint32_t> set;
        //robin_hood::unordered_set<uint32_t> set;
#endif
        set.max_load_factor(0.97f);

        while (set.size() < maxSize) {
            //auto key = dis(rng);
            auto key = srng();

            size_t prevSize = set.size();
            size_t prevCap = set.bucket_count();
            set.insert(key);
            if (set.bucket_count() > prevCap && prevCap > 0) {
                auto lf = static_cast<double>(prevSize) / prevCap;
                std::cout << prevCap << " " << prevSize << " " << lf << "\n";
            }
        }

        double time_use = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
        std::cout << " time = " << time_use << " sec\n\n";
    }

    return 0;
}
