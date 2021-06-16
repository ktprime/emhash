#include <string>
#include <chrono>
#include <unordered_map>
#include <random>
#include <iostream>

//#define EMH_HIGH_LOAD 201000
//#define EMH_FIBONACCI_HASH 1
//#define EMH_SAFE_HASH 1

#include "sfc64.h"

#include "tsl/robin_map.h"
#include "tsl/hopscotch_map.h"
#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"
#include "martin/robin_hood.h"
#include "emilib/emilib32.hpp"

#include "old/hash_table2.hpp"
#include "old/hash_table3.hpp"
#include "old/hash_table4.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table7.hpp"

#include "phmap/phmap.h"
#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
#include "hrd/hash_set7.h"
#endif
//#include "patchmap/patchmap.hpp"

#include "hash_set2.hpp"
#include "hash_set3.hpp"
#include "hash_set4.hpp"

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
            for (int32_t i = 0; i < 1000000; i++)
            {
                map.emplace(i, "abcd");
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
    double t_all = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - now).count();

    printf("%6.2f %6.2f insert %6.2f iter %6.2f find %zd result: %s|%.2f|map size %zd\n", 
            t_all, t_insert, t_iter, t_find, result, title, map.load_factor(), sizeof(Map));
}

static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);

static std::mt19937_64 generator(time(0));
static std::string get_random_alphanum_string(std::size_t size) {
    std::string str(size, '\0');

    assert(size < 4096);
    const auto comm_head = size % 4;
    //test common head
    for(std::size_t i = 0; i < comm_head; i++) {
        str[i] = ALPHANUMERIC_CHARS[i];
    }
    for(std::size_t i = comm_head; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}

static void buildRandString(int size, std::vector<std::string>& randdata, int str_min)
{
    std::mt19937_64 srng; srng.seed(size);
    for (int i = 0; i < size; i++)
        randdata.emplace_back(get_random_alphanum_string(srng() % 64 + str_min));
}

void bench_wyhash(int size, int str_min)
{
    std::vector<std::string> randdata;
    randdata.reserve(size + 10);
    emhash6::HashMap<std::string, int> ehash_map(size);

    for (int i = 0; i < 4; i++)
    {
        buildRandString(size + i, randdata, str_min);
        auto start = my_clock::now();
        auto t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();

        long sum = 0;
        for (auto& v : randdata) {
            sum += wyhash(v.data(), v.size(), 1);
        }

        t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
        printf("  align time use = %6.2lf sum = %ld\n", t_find, sum);

        //    ehash_map.clear();
        start = my_clock::now();
        sum = 0;
        for (auto& v : randdata) {
            //        ehash_map.emplace(v, 0);
            sum += wyhash(v.data() + 1, v.size(), 1);
        }

        t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
        printf("noalign time use = %6.2lf sum = %ld\n", t_find, sum);

        start = my_clock::now();
        sum = 0;
        for (auto& v : randdata)
            sum += std::hash<std::string>()(v);

        t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
        printf("stdhash time use = %6.2lf sum = %ld\n", t_find, sum);

        start = my_clock::now();
        sum = 0;
        for (auto& v : randdata)
            sum += robin_hood::hash<std::string>()(v);

        t_find = std::chrono::duration_cast<std::chrono::duration<double>>(my_clock::now() - start).count();
        printf("robin_hood time use = %6.2lf sum = %ld\n\n", t_find, sum);
    
    }
}

int main(int argc, char* argv[])
{
    bench<std::unordered_map<int, std::string>>("std::unordered_map");
    bench<robin_hood::unordered_node_map<int, std::string>>("robin_hood::unordered_node_map");
    bench<robin_hood::unordered_flat_map<int, std::string>>("robin_hood::unordered_flat_map");

    bench<emhash2::HashMap<int, std::string>>("emhash2::hashMap");
    bench<emhash3::HashMap<int, std::string>>("emhash3::hashMap");
    bench<emhash4::HashMap<int, std::string>>("emhash4::hashMap");
    bench<emhash5::HashMap<int, std::string>>("emhash5::hashMap");
    bench<emhash6::HashMap<int, std::string>>("emhash6::hashMap");
    bench<emhash6::HashMap<int, std::string, robin_hood::hash<int> >>("emhash6::hashMap");
    bench<emhash7::HashMap<int, std::string>>("emhash7::hashMap");

    bench<tsl::robin_map<int, std::string>>("tsl::robin_map");
    bench<tsl::hopscotch_map<int, std::string>>("tsl::hops_map");

    bench<phmap::flat_hash_map<int, std::string>>("phmap::hpmap");
    bench<phmap::node_hash_map<int, std::string>>("phmap::nodemap");

    bench<ska::flat_hash_map<int, std::string>>("ska::flat_map");
    bench<ska::bytell_hash_map<int, std::string>>("ska::byte_map");

#if __x86_64__ || _M_X64 || _M_IX86 || __i386__
    bench<hrd7::hash_map<int, std::string>>("hrd7::hash_map");
#endif
    bench<emilib3::HashMap<int, std::string>>("emilib3::hashMap");
//    bench<whash::patchmap<int, std::string>>("whash::patchmap");

    //bench_wyhash(3234567, 32);

    srand(time(0));

    if (argc > 4)
    {
        //phmap::flat_hash_map<int, std::string> mmap;
        robin_hood::unordered_node_map<int, std::string> mmap;
        //tsl::robin_map<int, std::string> mmap;
        //ska::flat_hash_map<int, std::string> mmap;
        for (int i = 0; i < 10000; i ++)
            mmap[rand()] = mmap[rand()];
    }

    size_t maxSize = 1U << 28;
    size_t numReps = 3;

    std::random_device rd;
    auto rng = std::mt19937{rd()};
    auto dis = std::uniform_int_distribution<uint32_t>{0, (1U << 31) - 1};
    //sfc64 srng(rd());

    for (size_t rep = 0; rep < numReps; ++rep) {
        auto start = my_clock::now();
#ifdef ET
        emhash2::HashSet<uint32_t> set;
#else
        ska::bytell_hash_set<uint32_t> set;
        //phmap::flat_hash_set<uint32_t> set;
        //robin_hood::unordered_set<uint32_t> set;
#endif
        set.max_load_factor(0.87f);

        while (set.size() < maxSize) {
            auto key = dis(rng);
            //auto key = srng();

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

