#include "util.h"

#if __linux__ && AVX2
#include <sys/mman.h>
#include "fht/fht_ht.hpp"
#endif


#if __GNUC__
//#include <ext/pb_ds/assoc_container.hpp>
#endif

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif
//#define EMH_INT_HASH 1

#include "martin/robin_hood.h"
#include "phmap/phmap.h"
#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"
#include "tsl/robin_map.h"
#include "tsl/hopscotch_map.h"

//#include "patchmap/patchmap.hpp"
//#include "emilib/emilib33.hpp"
#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

#include "hash_table8.hpp"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"

static std::random_device rd;
static std::mt19937_64 rnd(rd());

/*
    Key Types:
    INT32               -> uint32_t
    INT64               -> uint64_t
    STRING              -> std::string
        SET: $> make K=<option>


    K_LEN               -> length of key if type is string
        SET: $> make K_LEN=<option>

    Val Types:
    INT32               -> uint32_t
    INT64               -> uint64_t
    STRING              -> std::string
        SET: $> make V=<option>

    K_LEN               -> length of val if type is string
        SET: $> make V_LEN=<option>

    Test Params:
    INIT_SIZE           -> init size for tables
        SET: $> make ISIZE=<option>
    TEST_LEN            -> number of inserts
        SET: $> make LEN=<option>
    QUERY_RATE          -> queries per insert
        SET: $> make QR=<option>
    REMOVE_RATE         -> removes per insert
        SET: $> make RR=<option>
    INSERT_FAILURE_RATE -> % of inserts that should be succesful (unique)
        SET: $> make IFR=<option>
    QUERY_FAILURE_RATE  -> % of queries that should be succesful (key is in)
        SET: $> make QFR=<option>
    REMOVE_FAILURE_RATE -> % of removes that should be succesful (key is in)
        SET: $> make <RFR=<option>
 */

// expect either INT32, INT64 or STRING to be defined
#if not defined K_INT64
    const char* key_name = "uint64_t";
    typedef uint64_t test_key_t;
    #define KEY_LEN sizeof(test_key_t)
    test_key_t gen_key() {
        return rnd() * rnd() + rnd();
    }

#elif defined K_STRING
    const char* key_name = "string";
    typedef std::string test_key_t;
#if K_LEN == 0
    #define K_LEN 10
//static_assert( 0, "When using strings you must specify \"K_LEN=<Length of key string\"");
#endif
    #define KEY_LEN K_LEN
    test_key_t gen_key() {
        test_key_t new_key = "";
        for (uint32_t i = 0; i < KEY_LEN; i++) {
            new_key += rnd() % 256;
        }
        return new_key;
}

#else
    const char* key_name = "uint32_t";
    typedef uint32_t test_key_t;
    #define KEY_LEN sizeof(test_key_t)
    test_key_t gen_key() {
        return rnd();
    }
//static_assert( 0, "Make with either \"K=INT32\", \"K=INT64\", or \"K=STRING\". If Using " "\"K=STRING\" also be sure to set \"K_LEN=<desired length of string " "keys\"");
#endif

#if defined V_INT64
    const char* val_name = "uint64_t";
    typedef uint64_t   test_val_t;
    typedef test_val_t val_sink_t;
    #define VAL_LEN sizeof(test_val_t)
    test_val_t gen_val() {
        return rnd() * rnd() + rnd();
    }

#elif defined V_STRING
    const char* val_name = "string";
    typedef std::string  test_val_t;
    typedef test_val_t * val_sink_t;

#if V_LEN == 0
#define V_LEN 10
//static_assert( 0, "When using strings you must specify \"V_LEN=<Length of val string\"");
#endif

#define VAL_LEN V_LEN
    test_val_t gen_val() {
        test_val_t new_val = "";
        for (uint32_t i = 0; i < VAL_LEN; i++) {
            new_val += rnd() % 256;
        }
        return new_val;
    }

#else
    const char* val_name = "uint32_t";
    typedef uint32_t   test_val_t;
    typedef test_val_t val_sink_t;
    #define VAL_LEN sizeof(test_val_t)

    test_val_t gen_val() {
        return rnd();
    }

//static_assert(0, "Make with either \"V=INT32\", \"V=INT64\", or " "\"V=STRING\". If Using \"V=STRING\" also be sure to set " "\"V_LEN=<desired length of string keys\"");
#endif

#ifndef TEST_LEN
static int TEST_LEN = 10456789;
#endif

#ifndef INIT_SIZE
static int INIT_SIZE = 4096;
#endif

#ifndef QUERY_RATE
static int QUERY_RATE = 2;
#endif

#ifndef REMOVE_RATE
static double REMOVE_RATE = 0.5;
#endif

#ifndef INSERT_FAILURE_RATE
static double INSERT_FAILURE_RATE = 0.35;
#endif

#ifndef QUERY_FAILURE_RATE
static double QUERY_FAILURE_RATE = 0.3;
#endif

#ifndef REMOVE_FAILURE_RATE
static double REMOVE_FAILURE_RATE = 0.25;
#endif

static void report(double ns_diff, const char * header, float lf, int sum);

static int64_t inline now2ns()
{
    return getus() * 1000;
}

inline static uint32_t
rnd_above_perc(int r) {
    return rnd() % RAND_MAX > r;
}

static void
init_keys(std::vector<test_key_t> & insert_keys) {
    insert_keys.clear();
    for (uint32_t i = 0; i < (uint32_t)TEST_LEN; i++) {
        // duplicates here mean nothing...
        insert_keys.push_back(gen_key());
    }
}

static void
init_query_keys(std::vector<test_key_t> & insert_keys, std::vector<test_key_t> & query_keys) {
    query_keys.clear();
    uint32_t query_fail = QUERY_FAILURE_RATE * RAND_MAX;
    for (uint32_t i = 0; i < TEST_LEN * QUERY_RATE; i++) {
        if (rnd_above_perc(query_fail)) {
            query_keys.push_back(insert_keys[rnd() % TEST_LEN]);
        }
        else {
            // assuming rndomly generated key will not be found
            query_keys.push_back(gen_key());
        }
    }
}

static void
init_remove_keys(std::vector<test_key_t> & insert_keys, std::vector<test_key_t> & remove_keys) {
    remove_keys.clear();
    uint32_t upper_bound = (TEST_LEN * REMOVE_RATE);
    uint32_t remove_fail = REMOVE_FAILURE_RATE * RAND_MAX;
    for (uint32_t i = 0; i < upper_bound; i++) {
        if (rnd_above_perc(remove_fail)) {
            remove_keys.push_back(insert_keys[rnd() % TEST_LEN]);
        }
        else {
            // assuming rndomly generated key will not be found
            remove_keys.push_back(gen_key());
        }
    }
}

static void
clear_cache() {

#if __linux__ && AVX2
    // not too woried about instruction cache... just clear data cache
    uint32_t *     ptrs[10];
    const uint32_t clear_size = (1 << 22);
    for (uint32_t i = 0; i < 10; i++) {

        // do this in a way that doesn't mess with heap too much
        ptrs[i] = (uint32_t *)mmap(NULL,
                                   clear_size * sizeof(uint32_t),
                                   PROT_READ | PROT_WRITE,
                                   MAP_ANONYMOUS | MAP_PRIVATE,
                                   -1,
                                   0);
        assert(ptrs[i] != MAP_FAILED);
        for (uint32_t j = 0; j < clear_size; j++) {
            ptrs[i][j] = rnd();
        }
    }
    for (uint32_t i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < clear_size; j++) {
            ptrs[i][j] += rnd();
        }
        assert(!munmap(ptrs[i], clear_size * sizeof(uint32_t)));
    }
#else


#endif
}

//https://github.com/goldsteinn/hashtable_test
template<typename ht>
static int run_table(std::vector<test_key_t> & insert_keys,
               std::vector<test_val_t> & insert_vals,
               std::vector<test_key_t> & query_keys,
               std::vector<test_key_t> & remove_keys) {

    clear_cache();
    ht test_table;//(INIT_SIZE > 10 ? INIT_SIZE : insert_keys.size() / INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    auto start_time = now2ns();
    auto sum = 0;
    auto dvalue = gen_val();
    for (uint32_t i = 0; i < (uint32_t)TEST_LEN; i++) {
        test_table[insert_keys[i]] = dvalue;
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            sum += test_table.count(query_keys[j]);
        }
        if (i == next_remove) {
            auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
            if (remove_iter >= remove_keys.size())
                remove_iter = 1;
            sum += sink;
        }
    }
    auto end_time = now2ns();

    auto map_name = std::string(typeid(ht).name());
    if (map_name.find("Im") != std::string::npos)
        map_name = map_name.substr(2, map_name.find("Im") - 2);
    else if (map_name.find("Ij") != std::string::npos)
        map_name = map_name.substr(2, map_name.find("Ij") - 2);
    else if (map_name.find("<") != std::string::npos)
        map_name = map_name.substr(0, map_name.find("<"));

    report(end_time - start_time, map_name.data(), test_table.load_factor(), sum);
    return sum;
//    printf("sum = %d lf = %.2f\n", sum, test_table.load_factor());
}

static void
report(double ns_diff, const char * header, float lf, int sum) {

    const uint32_t total_ops = (uint32_t)(TEST_LEN * (1 + QUERY_RATE + REMOVE_RATE));
    static int once = 0;
    if (once++ == 0) {
        fprintf(stderr, "Total Operations: %d\n", total_ops);
        fprintf(stderr,
                "\t\tInserts (%d), Failure Rate (%.3f)\n",
                TEST_LEN,
                (float)INSERT_FAILURE_RATE);
        fprintf(stderr,
                "\t\tQuerys  (%d), Failure Rate (%.3f)\n",
                QUERY_RATE * TEST_LEN,
                (float)QUERY_FAILURE_RATE);
        fprintf(stderr,
                "\t\tRemoves (%d), Failure Rate (%.3f)\n\n",
                (uint32_t)(REMOVE_RATE * TEST_LEN),
                (float)REMOVE_FAILURE_RATE);
    }

    fprintf(stderr, "%s -> \n", header);
    const uint64_t ns_mult = 1000 * 1000 * 1000;

    if (ns_diff > ns_mult*10)
        fprintf(stderr, "\t%.4lf Sec", ns_diff / ns_mult);
    if (ns_diff > ns_mult / 100)
        fprintf(stderr, "\t%.3lf MS ", ns_diff / (ns_mult / 1000));

    fprintf(stderr, "\t%.2lf US", ns_diff / (ns_mult / (1000 * 1000)));
//    fprintf(stderr, "\t%.2lf NS ", ns_diff);
    fprintf(stderr, " -> load factor = %.2f, sum = %d, ns / op = %.1lf\n\n", lf, sum, ns_diff / total_ops);
}


void test_delay()
{
    const int LEN = 256 * 1024 * 1024;

    for (int len = 1; len < 20000; len *= 2)
    {
        auto start_time = now2ns();
        int* arr2 = new int[LEN]; int sum = 1;
        for (int i = 1; i < LEN; i += len) sum += arr2[i];
        auto end_time = now2ns();
        printf("time[%5d] use %4d ms [%d] ns / op = %.2lf\n", len, int(end_time - start_time) / 1000000, sum, (double)(end_time - start_time) / (LEN / len));
#if 0
        start_time = now2ns();
        for (int i = 1; i < LEN; i += len) sum += arr2[i];
        end_time = now2ns();
        printf("time[%5d] use %4ld ms [%d] ns / op = %.2lf\n\n", len, (end_time - start_time) / 1000000, sum, (double)(end_time - start_time) / (LEN / len));
#endif
        delete[] arr2;
    }

    //    start_time = now2ns();
    //    for (int i = 1; i < LEN; i += 16)
    //    arr2[i] += 11;
    //    printf("time2 use %ld ms\n", (now2ns() - start_time) / 1000000);
}


static inline uint32_t hash32(uint32_t key)
{
#if H32 == 6
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;

#elif H32 == 0
    uint64_t r = key * UINT64_C(0xca4bcaa75ec3f625);
    return (r >> 32) + r;
#elif H32 == 2
    //MurmurHash3Mixer
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccd;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53;
    h ^= h >> 33;
    return (h >> 32) + h;
#elif H32 == 3
    uint64_t x = key;
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return (x >> 32) + x;
#else
    return key;
#endif
}

struct Hash32 {
    inline size_t operator()(uint32_t key) const {
        return hash32(key);
    }
};

static inline uint32_t get_key(const uint32_t n, const uint32_t x)
{
    return hash32(x % (n >> 2));
}

template<typename ht>
static uint32_t test_int(const uint32_t n, const uint32_t x0)
{
    uint32_t i, x, z = 0;
    ht h;
    h.max_load_factor(0.90);
    for (i = 0, x = x0; i < n; ++i) {
        x = hash32(x);
#ifndef UDB2_TEST_DEL
        z += ++h[get_key(n, x)];
#else
        auto p = h.emplace(get_key(n, x), i);
        if (p.second == false) h.erase(p.first);
#endif
    }
//    fprintf(stderr, "# unique keys: %ld; checksum: %u\n", h.size(), z);
    return h.load_factor() * 100;
}

auto static rda = now2ns() % 4096;
//https://github.com/attractivechaos/udb2
template<typename ht>
int run_udb2(const char* str)
{
    uint32_t m = 5, max = 50000000, n = 10000000 + rda;
    const auto step = (max - n) / m;

    auto now = now2ns();
    for (int i = 0; i <= m; ++i, n += step) {
        const uint32_t x0 = i + 1;
        auto t = now2ns();
        auto lf = test_int<ht>(n, x0);
        t = now2ns() - t;
        printf("    %d\t%.3f\t\t%.2f\t  0.%2d\n", n, t / 1000000000.0, (double)t / n, lf);
    }

    printf("%s : %.2lf sec\n\n", str, (now2ns() - now) / 1000000000.0);
    return 0;
}

int main(int argc, char* argv[])
{
    srand(time(0));

    //test_delay();
    if (argc == 1)
    {
        run_udb2<emhash8::HashMap<uint32_t, uint32_t, Hash32>>("emhash8");
//        run_udb2<whash::patchmap<uint32_t, uint32_t, Hash32>>("patchmap");
        run_udb2<ska::flat_hash_map<uint32_t, uint32_t, Hash32>>("ska_flat");
        run_udb2<ska::bytell_hash_map<uint32_t, uint32_t, Hash32>>("ska_byte");
        run_udb2<emhash7::HashMap<uint32_t, uint32_t, Hash32>>("emhash7");
        run_udb2<emhash6::HashMap<uint32_t, uint32_t, Hash32>>("emhash6");
        run_udb2<robin_hood::unordered_flat_map<uint32_t, uint32_t, Hash32>>("martin_flat");
        run_udb2<phmap::flat_hash_map<uint32_t, uint32_t, Hash32>>("phmap_flat");
        run_udb2<tsl::robin_map<uint32_t, uint32_t, Hash32>>("tsl_robin");
        run_udb2<tsl::hopscotch_map<uint32_t, uint32_t, Hash32>>("tsl_hops");
        run_udb2<emhash5::HashMap<uint32_t, uint32_t, Hash32>>("emhash5");
        run_udb2<emilib::HashMap<uint32_t, uint32_t, Hash32>>("emilib");
        run_udb2<emilib2::HashMap<uint32_t, uint32_t, Hash32>>("emilib2");
        run_udb2<emilib3::HashMap<uint32_t, uint32_t, Hash32>>("emilib3");
#if HAVE_BOOST
        run_udb2<boost::unordered_flat_map<uint32_t, uint32_t, Hash32>>("boost");
#endif

#if ABSL_HMAP
        run_udb2<absl::flat_hash_map<uint32_t, uint32_t, Hash32>>("absl");
#endif
#if QC_HASH
        run_udb2<qc::hash::RawMap<uint32_t, uint32_t, Hash32>>("qchash");
        run_udb2<fph::DynamicFphMap<uint32_t, uint32_t, fph::MixSeedHash<uint32_t>>>("fph");
#endif

#if __linux__ && AVX2
        run_udb2<fht_table<uint32_t, uint32_t>>("fht_table");
#endif
#if __GNUC__
      //  run_udb2<__gnu_pbds::gp_hash_table<uint32_t, uint32_t>>("gp_hashtable");
#endif
    }

    std::vector<test_key_t> insert_keys;
    std::vector<test_val_t> insert_vals;
    std::vector<test_key_t> query_keys;
    std::vector<test_key_t> remove_keys;

#ifndef INSERT_FAILURE_RATE
    for (int i = 1; i + 1 < argc; i++) {
        std::string cmd = argv[i];
        if (cmd == "if")
            INSERT_FAILURE_RATE = atof(argv[i + 1]);
        else if (cmd == "rf")
            REMOVE_FAILURE_RATE = atof(argv[i + 1]);
        else if (cmd == "qf")
            QUERY_FAILURE_RATE  = atof(argv[i + 1]);
        else if (cmd == "rr")
            REMOVE_RATE = atof(argv[i + 1]);
        else if (cmd == "qr")
            QUERY_RATE = atof(argv[i + 1]);
        else if (cmd == "n")
            TEST_LEN = atoi(argv[i + 1]);
        else if (cmd == "i")
            INIT_SIZE = atoi(argv[i + 1]);
    }
#endif

    fprintf(stderr, "key=%s,value=%s\nrf = %.2lf\nqf = %.2lf\nrr = %.2lf\nqr = %d\nn  = %d\ni  = %d\n\n",
            key_name, val_name, REMOVE_FAILURE_RATE, QUERY_FAILURE_RATE, REMOVE_RATE, QUERY_RATE, TEST_LEN, INIT_SIZE);

#ifdef HOOD_HASH
    using hash_t = robin_hood::hash<int64_t>;
#elif ABSL_HASH
    using hash_t = absl::Hash<int64_t>;
#elif FIB_HASH
    using hash_t = Int64Hasher<int64_t>;
#else
    using hash_t = std::hash<int64_t>;
#endif

    while (true) {
    init_keys(insert_keys);
    init_query_keys(insert_keys, query_keys);
    init_remove_keys(insert_keys, remove_keys);

    run_table<emhash8::HashMap<test_key_t, test_val_t, hash_t>>  (insert_keys, insert_vals, query_keys, remove_keys);
#if __linux__ && AVX2
    run_table<fht_table<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
#endif


#if ABSL_HMAP
    run_table <absl::flat_hash_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
#endif


    run_table <std::unordered_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <ska::flat_hash_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <ska::bytell_hash_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <emhash5::HashMap<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <emilib::HashMap<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
    run_table <emilib2::HashMap<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
    run_table <emilib3::HashMap<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);

#if HAVE_BOOST
    run_table <boost::unordered_flat_map<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
#endif

    run_table <emhash6::HashMap<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <emhash7::HashMap<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <robin_hood::unordered_flat_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
//    run_table <robin_hood::unordered_node_map<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <phmap::flat_hash_map<test_key_t, test_val_t, hash_t>> (insert_keys, insert_vals, query_keys, remove_keys);
//    run_table <phmap::node_hash_map<test_key_t, test_val_t, hash_t>> (insert_keys, insert_vals, query_keys, remove_keys);
    run_table <tsl::robin_map<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
    run_table <tsl::hopscotch_map<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
//    run_table <whash::patchmap<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
#if __GNUC__
    // run_table <__gnu_pbds::gp_hash_table<test_key_t, test_val_t, hash_t>> (insert_keys, insert_vals, query_keys, remove_keys);
#endif

#if QC_HASH
    run_table <qc::hash::RawMap<test_key_t, test_val_t, hash_t>>(insert_keys, insert_vals, query_keys, remove_keys);
    run_table <fph::DynamicFphMap<test_key_t, test_val_t, fph::MixSeedHash<test_key_t>>>(insert_keys, insert_vals, query_keys, remove_keys);
#endif

//    run_table <emilib4::HashMap<test_key_t, test_val_t, hash_t>>     (insert_keys, insert_vals, query_keys, remove_keys);
    int n = TEST_LEN;
    printf(">> "); scanf("%u", &n);
    if (n < 0)
        break;
    else if (n < 10 && n > 0)
        TEST_LEN *= n;
    else if (n > 100000)
        TEST_LEN = n;
    }

    return 0;
}
