#include <string>
#include <chrono>
#include <unordered_map>
#include <random>
#include <iostream>

//#define EMH_HIGH_LOAD 201000
//#define EMH_INT_HASH 1
//#define EMH_SAFE_HASH 1

#include "sfc64.h"

#include "tsl/robin_map.h"
#include "tsl/hopscotch_map.h"
#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"
#include "martin/robin_hood.h"

#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"

#include "emhash/hash_table2.hpp"
#include "emhash/hash_table3.hpp"
#include "emhash/hash_table4.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table7.hpp"

#include "phmap/phmap.h"
#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#include "hrd/hash_set7.h"
#endif
//#include "patchmap/patchmap.hpp"

#if QC_HASH
#include "qchash/qc-hash.hpp"
#include "fph/dynamic_fph_table.h"
#endif

#if ABSL_HMAP
#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/raw_hash_set.cc"

#include "absl/hash/internal/low_level_hash.cc"
#include "absl/hash/internal/hash.cc"
#include "absl/hash/internal/city.cc"
#endif


#include "hash_set2.hpp"
#include "hash_set3.hpp"
#include "hash_set4.hpp"

using k_value = int;

using my_clock = std::chrono::high_resolution_clock;

template<typename Map>
void bench(char const* title)
{
    int trials = 100;
    size_t result = 0;

    Map map;

//    map.max_load_factor(7.0/8);
    // Insert
    auto start = my_clock::now();
    auto now = start;
    {
        for (int i = 0; i < 10; ++i) {
            map.clear();
            for (int32_t j = 0; j < 1000000; j++)
            {
                map.emplace(i + j, j);
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
                result += keyVal.second;
            }
        }
    }
    double t_iter = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();

    // Find
    start = my_clock::now();
    {
        for (size_t x = 0; x < 10; ++x) {
            for (int32_t i = 0; i < 5000000; i++) {
                result = map.count(i);
            }
        }
    }
    double t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
    double t_all = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - now).count();

    printf("%6.2f %6.2f insert %6.2f iter %6.2f find %zd result: %s|%.2f|map size %zd\n",
            t_all, t_insert, t_iter, t_find, result, title, map.load_factor(), sizeof(Map));
}

int main(int argc, char* argv[])
{
    bench<std::unordered_map<int, k_value>>("std::unordered_map");
    bench<robin_hood::unordered_node_map<int, k_value>>("robin_hood::unordered_node_map");
    bench<robin_hood::unordered_flat_map<int, k_value>>("robin_hood::unordered_flat_map");

    bench<emilib::HashMap<int, k_value>>("emilib::hashMap");
    bench<emilib2::HashMap<int, k_value>>("emilib2::hashMap");
#if QC_HASH
    bench<fph::DynamicFphMap<int, k_value, fph::MixSeedHash<int>>>("fph::mixmap");
    //bench<qc::hash::RawMap<int, k_value>>("qc::hashmap");
#endif
#if ABSL_HMAP
    bench<absl::flat_hash_map<int, k_value>>("absl::flat_hash_map");
#endif

    bench<emhash2::HashMap<int, k_value>>("emhash2::hashMap");
    bench<emhash3::HashMap<int, k_value>>("emhash3::hashMap");
    bench<emhash4::HashMap<int, k_value>>("emhash4::hashMap");

    bench<emhash5::HashMap<int, k_value>>("emhash5::hashMap");
    bench<emhash6::HashMap<int, k_value>>("emhash6::hashMap");
    bench<emhash6::HashMap<int, k_value, robin_hood::hash<int> >>("emhash6::hashMap");
    bench<emhash7::HashMap<int, k_value>>("emhash7::hashMap");

    bench<tsl::robin_map<int, k_value>>("tsl::robin_map");
    bench<tsl::hopscotch_map<int, k_value>>("tsl::hops_map");

    bench<phmap::flat_hash_map<int, k_value>>("phmap::hpmap");
    bench<phmap::node_hash_map<int, k_value>>("phmap::nodemap");

    bench<ska::flat_hash_map<int, k_value>>("ska::flat_map");
    bench<ska::bytell_hash_map<int, k_value>>("ska::byte_map");

#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
    bench<hrd7::hash_map<int, k_value>>("hrd7::hash_map");
#endif
//    bench<emilib3::HashMap<int, k_value>>("emilib3::hashMap");
//    bench<whash::patchmap<int, k_value>>("whash::patchmap");

    //bench_wyhash(3234567, 32);

    srand(time(0));

    if (argc > 4)
    {
        //phmap::flat_hash_map<int, k_value> mmap;
        robin_hood::unordered_node_map<int, k_value> mmap;
        //tsl::robin_map<int, k_value> mmap;
        //ska::flat_hash_map<int, k_value> mmap;
        for (int i = 0; i < 10000; i ++)
            mmap[rand()] = mmap[rand()];
    }

    return 0;
}

