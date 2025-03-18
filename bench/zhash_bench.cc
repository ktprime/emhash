#include <cassert>
#include <cstdio>
#include <chrono>
#include <random>
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#if QC_HASH
#include "qchash/qc-hash.hpp"
#include "fph/dynamic_fph_table.h"
#endif

#if CXX20 || __cplusplus > 201703L
#include "jg/dense_hash_map.hpp" //https://github.com/Jiwan/dense_hash_map
#endif
#include "rigtorp/rigtorp.hpp" //https://github.com/Jiwan/dense_hash_map

#if ABSL
//#define _HAS_DEPRECATED_RESULT_OF 1
#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/raw_hash_set.cc"
//#endif

//#if ABSL_HASH
#include "absl/hash/internal/low_level_hash.cc"
#include "absl/hash/internal/hash.cc"
#include "absl/hash/internal/city.cc"
#endif

#include "tsl/robin_map.h"
#include "tsl/hopscotch_map.h"
#include "tsl/bhopscotch_map.h"
//#include "tsl/array_map.h"
#include "tsl/ordered_map.h"
#include "tsl/sparse_map.h"
//#include "tsl/htrie_map.h"

#include "google/dense_hash_map"
#include "zhashmap/hashmap.h"
#include "zhashmap/linkedhashmap.h"
#include "hash_table7.hpp"
#include "hash_table8.hpp"
#include "hash_table5.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"
#include "martin/robin_hood.h"
#include "phmap/phmap.h"
#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"

using namespace std::chrono;

#ifndef _WIN32
std::string get_proc_info(const char *key)
{
    FILE *file;
    char buf[1024];
    char *line;

    if (!(file = fopen("/proc/cpuinfo", "r"))) {
        return "";
    }

    while ((line = fgets(buf, sizeof(buf), file)) != nullptr) {
        std::string s(line);
        if (s.find(key) != std::string::npos) {
            size_t c = s.find(":");
            return s.substr(c + 2, s.size() - c - 3);
        }
    }

    fclose(file);
    return "";
}

std::string get_cpu_model()
{
    std::string s = get_proc_info("model name");
    size_t c = s.find("@");
    return c != std::string::npos ? s.substr(0, c-1) : s;
}
#endif

struct rng
{
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_int_distribution<uint64_t> random_dist;

    template <typename T>
    T get() { return random_dist(random_engine); }
};

template <typename K, typename V>
std::vector<std::pair<K,V>> get_random(size_t count)
{
    rng r;
    std::vector<std::pair<K,V>> data;
    zedland::hashmap<K,char> set;
    for (size_t i = 0; i < count; i++) {
        K key = r.get<K>();
        while (set[key] == 1) const_cast<K&>(key) = r.get<K>();
        V val = r.get<V>();
        data.push_back(std::pair<K,V>(key, val));
        set[key] = 1;
    };
    return data;
}

template <typename T>
void print_timings(const char *name, T t1, T t2, T t3, T t4, T t5, T t6, size_t count)
{
    static int maps = 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "_%s::insert [%2d]", name, ++maps);
    printf("|%-40s|%8s|%12zu|%8.1f|\n", buf, "random", count,
        duration_cast<nanoseconds>(t2-t1).count()/(float)count);
    snprintf(buf, sizeof(buf), "_%s::clear_", name);
    printf("|%-40s|%8s|%12zu|%8.1f|\n", buf, "random", count,
        duration_cast<nanoseconds>(t3-t2).count()/(float)count);
    snprintf(buf, sizeof(buf), "_%s::insert_", name);
    printf("|%-40s|%8s|%12zu|%8.1f|\n", buf, "random", count,
        duration_cast<nanoseconds>(t4-t3).count()/(float)count);
    snprintf(buf, sizeof(buf), "_%s::lookup_", name);
    printf("|%-40s|%8s|%12zu|%8.1f|\n", buf, "random", count,
        duration_cast<nanoseconds>(t5-t4).count()/(float)count);
    snprintf(buf, sizeof(buf), "_%s::erase_", name);
    printf("|%-40s|%8s|%12zu|%8.1f|\n", buf, "random", count,
        duration_cast<nanoseconds>(t6-t5).count()/(float)count);
    printf("|%-40s|%8s|%12s|%8s|%8.2f\n", "-", "-", "-", "-",
            duration_cast<nanoseconds>(t6-t1).count()/(float)count);
}

static const size_t sizes[] = { 1023, 16383, 65535, 1048575, 0 };

template <typename Map>
float bench_spread(const char *name, size_t count, size_t spread)
{
    Map map;
    auto t1 = system_clock::now();
    for (size_t i = 0; i < count; i++) {
        map[i&spread]++;
    }

    auto t2 = duration_cast<nanoseconds>(system_clock::now() - t1).count() / float(count);
    char buf[128];
    snprintf(buf, sizeof(buf), "_%s_", name);
    printf("|%-40s|%8zu|%12zu|%8.1f|\n", buf, spread, count, t2);
    return t2;
}

template <typename Map>
void bench_spread(const char *name, size_t count)
{
    float lf = 0.0;
    for (const size_t *s = sizes; *s != 0; s++) {
        lf += bench_spread<Map>(name,count,*s);
    }
    printf("|%-40s|%8s|%12s|%8s|%7.1f\n", "-", "-", "-", "-", lf);
}

template <typename Map>
void bench_spread_google(const char *name, size_t count, size_t spread)
{
    Map map;
    map.set_empty_key(-1);
    auto t1 = system_clock::now();
    for (size_t i = 0; i < count; i++) {
        map[i&spread]++;
    }
    auto t2 = system_clock::now();
    char buf[128];
    snprintf(buf, sizeof(buf), "_%s_", name);
    printf("|%-40s|%8zu|%12zu|%8.1f|\n", buf, spread, count,
        duration_cast<nanoseconds>(t2-t1).count()/(float)count);
}

template <typename Map>
void bench_spread_google(const char *name, size_t count)
{
    for (const size_t *s = sizes; *s != 0; s++) {
        bench_spread_google<Map>(name,count,*s);
    }
    printf("|%-40s|%8s|%12s|%8s|\n", "-", "-", "-", "-");
}

template <typename Map>
void bench_map(const char* name, size_t count)
{
    typedef std::pair<typename Map::key_type,typename Map::mapped_type> pair_type;

    Map ht;

    auto data = get_random<typename Map::key_type,typename Map::mapped_type>(count);
    auto t1 = system_clock::now();
    for (auto &ent : data) {
        ht.emplace(ent.first, ent.second);
    }
    auto t2 = system_clock::now();
    ht.clear();
    auto t3 = system_clock::now();
    for (auto &ent : data) {
        ht.emplace(ent.first, ent.second);
    }
    auto t4 = system_clock::now();
    for (auto &ent : data) {
        assert(ht.find(ent.first)->second == ent.second);
    }
    auto t5 = system_clock::now();
    for (auto &ent : data) {
        ht.erase(ent.first);
    }
    auto t6 = system_clock::now();

    print_timings(name, t1, t2, t3, t4, t5, t6, count);
}

template <typename Map>
void bench_map_google(const char* name, size_t count)
{
    typedef std::pair<typename Map::key_type,typename Map::mapped_type> pair_type;

    Map ht;
    ht.set_empty_key(-1);
    ht.set_deleted_key(-2);

    auto data = get_random<typename Map::key_type,typename Map::mapped_type>(count);
    auto t1 = system_clock::now();
    for (auto &ent : data) {
        ht.insert(ht.end(), pair_type(ent.first, ent.second));
    }
    auto t2 = system_clock::now();
    ht.clear();
    auto t3 = system_clock::now();
    for (auto &ent : data) {
        ht.insert(ht.end(), pair_type(ent.first, ent.second));
    }
    auto t4 = system_clock::now();
    for (auto &ent : data) {
        assert(ht.find(ent.first)->second == ent.second);
    }
    auto t5 = system_clock::now();
    for (auto &ent : data) {
        ht.erase(ent.first);
    }
    auto t6 = system_clock::now();

    print_timings(name, t1, t2, t3, t4, t5, t6, count);
}

void heading()
{
    printf("\n");
    printf("|%-40s|%8s|%12s|%8s|%8s|\n",
        "container", "spread", "count", "time_ns", "total_ns");
    printf("|%-40s|%8s|%12s|%8s|\n",
        ":--------------------------------------",
        "-----:", "----:", "------:");
}

int main(int argc, char **argv)
{
    size_t count = 1000000;

    if (argc == 2) {
        count = atoi(argv[1]);
    }

    printf("benchmark: tsl::robin_map, zedland::hashmap, google::dense_hash_map, absl::flat_hash_map\n");
#ifndef _WIN32
    printf("cpu_model: %s\n", get_cpu_model().c_str());
#endif

    heading();
    bench_spread<std::unordered_map<size_t,size_t>>("std::unordered_map::operator[]",count);
    bench_spread<tsl::robin_map<size_t,size_t>>("tsl::robin_map::operator[]",count);
    bench_spread<tsl::hopscotch_map<size_t,size_t>>("tsl::hopscotch_map::operator[]",count);
    bench_spread<tsl::bhopscotch_map<size_t,size_t>>("tsl::bhopscotch_map::operator[]",count);
    bench_spread<tsl::ordered_map<size_t,size_t>>("tsl::ordered_map::operator[]",count);
    bench_spread<tsl::sparse_map<size_t,size_t>>("tsl::sparse_map::operator[]",count);
//    bench_spread<tsl::array_map<size_t,size_t>>("tsl::array_map::operator[]",count);
//    bench_spread<tsl::htrie_map<size_t,size_t>>("tsl::htrie_map::operator[]",count);

    bench_spread<zedland::hashmap<size_t,size_t>>("zedland::hashmap::operator[]",count);
    bench_spread<zedland::linkedhashmap<size_t,size_t>>("zedland::linkedhashmap::operator[]",count);
#if ABSL
    bench_spread<absl::flat_hash_map<size_t,size_t>>("absl::flat_hash_map::operator[]",count);
#endif

#if CXX20 || __cplusplus > 201703L
    bench_spread<jg::dense_hash_map<size_t,size_t>>("jg::dense_hash_map::operator[]",count);
#endif
    //bench_spread<rigtorp::HashMap<size_t,size_t>>("rigtorp::HashMap::operator[]",count);

#if QC_HASH
    bench_spread<qc::hash::RawMap<size_t,size_t>>("qc::hash::RawMap::operator[]",count);
    bench_spread<fph::DynamicFphMap<size_t,size_t,fph::MixSeedHash<size_t>>>("fph::DynamicFph::operator[]",count);
#endif

    bench_spread<emhash5::HashMap<size_t,size_t>>("emhash5::HashMap::operator[]",count);
    bench_spread<emhash7::HashMap<size_t,size_t>>("emhash7::HashMap::operator[]",count);
    bench_spread<emilib2::HashMap<size_t,size_t>>("emilib2::HashMap::operator[]",count);
    bench_spread<emilib3::HashMap<size_t,size_t>>("emilib3::HashMap::operator[]",count);
    bench_spread<robin_hood::unordered_flat_map<size_t,size_t>>("martin::flat_map::operator[]",count);
    bench_spread<phmap::flat_hash_map<size_t,size_t>>("phmap::flat_hash_map::operator[]",count);

    heading();
    bench_map<std::unordered_map<size_t,size_t>>("std::unordered_map", count);
    bench_map<tsl::robin_map<size_t,size_t>>("tsl::robin_map", count);
    bench_map<tsl::hopscotch_map<size_t,size_t>>("tsl::hopscotch_map::",count);
    bench_map<tsl::bhopscotch_map<size_t,size_t>>("tsl::bhopscotch_map::",count);
//    bench_map<tsl::ordered_map<size_t,size_t>>("tsl::ordered_map::",count);
    bench_map<tsl::sparse_map<size_t,size_t>>("tsl::sparse_map::",count);
//    bench_spread<tsl::array_map<size_t,size_t>>("tsl::array_map::",count);

    bench_map<zedland::hashmap<size_t,size_t>>("zedland::hashmap", count);
    bench_map<zedland::linkedhashmap<size_t,size_t>>("zedland::linkedhashmap", count);
#if ABSL
    bench_map_google<google::dense_hash_map<size_t,size_t>>("google::dense_hash_map", count);
    bench_map<absl::flat_hash_map<size_t,size_t>>("absl::flat_hash_map",count);
#endif

#if HAVE_BOOST
    bench_map<boost::unordered_flat_map<size_t,size_t>>("boost::flat_hash_map",count);
#endif

    bench_map<emhash5::HashMap<size_t,size_t>>("emhash5::HashMap",count);
    bench_map<emhash7::HashMap<size_t,size_t>>("emhash7::HashMap",count);
    bench_map<emilib2::HashMap<size_t,size_t>>("emilib2::HashMap",count);
    bench_map<emilib3::HashMap<size_t,size_t>>("emilib2::HashMap",count);
    //bench_map<robin_hood::unordered_node_map<size_t,size_t>>("martin::node_hash",count);
    bench_map<phmap::flat_hash_map<size_t,size_t>>("phmap::flat_hash_hash",count);
    bench_map<phmap::node_hash_map<size_t,size_t>>("phmap::node_hash_hash",count);
    bench_map<ska::flat_hash_map<size_t,size_t>>("ska::flat_hash_hash",count);
    bench_map<ska::bytell_hash_map<size_t,size_t>>("ska::bytell_hash_map",count);

#if CXX20 || __cplusplus > 201703L
    bench_map<jg::dense_hash_map<size_t,size_t>>("jg::dense_hash_map",count);
#endif

    bench_map<rigtorp::HashMap<size_t,size_t>>("rigtorp::HashMap",count);
    bench_map<emhash8::HashMap<size_t,size_t>>("emhash8::HashMap",count);

#if QC_HASH
    bench_map<qc::hash::RawMap<size_t,size_t>>("qc::hash::RawMap",count);
    bench_map<fph::DynamicFphMap<size_t,size_t, fph::SimpleSeedHash<size_t>>>("fph::DynamicFphMap",count);
#endif
    return 0;
}

