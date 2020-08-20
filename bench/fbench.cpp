#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <vector>

#if __linux__ && __x86_64__
#include <sys/mman.h>
#include "fht/fht_ht.hpp"
#endif

#include "ska/flat_hash_map.hpp"
#include "martin/robin_hood.h"
#include "phmap/phmap.h"
#include "tsl/robin_map.h"

#include "hash_table5.hpp"
#include "hash_table7.hpp"
#include "hash_table6.hpp"


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
#if defined K_INT64
    typedef uint64_t test_key_t;
    #define KEY_LEN sizeof(test_key_t)
    test_key_t gen_key() {
    return rand() * rand() + rand();
}

#elif defined K_STRING
    typedef std::string test_key_t;
#if K_LEN == 0
    #define K_LEN 10
//static_assert( 0, "When using strings you must specify \"K_LEN=<Length of key string\"");
#endif
    #define KEY_LEN K_LEN
    test_key_t gen_key() {
    test_key_t new_key = "";
    for (uint32_t i = 0; i < KEY_LEN; i++) {
        new_key += rand() % 256;
    }
    return new_key;
}

#else
//    #define K_INT32
    typedef uint32_t test_key_t;
    #define KEY_LEN sizeof(test_key_t)
    test_key_t gen_key() {
    return rand();
}
//static_assert( 0, "Make with either \"K=INT32\", \"K=INT64\", or \"K=STRING\". If Using " "\"K=STRING\" also be sure to set \"K_LEN=<desired length of string " "keys\"");
#endif

#if defined V_INT64
    typedef uint64_t   test_val_t;
    typedef test_val_t val_sink_t;
#define VAL_LEN sizeof(test_val_t)
    test_val_t
    gen_val() {
        return rand() * rand() + rand();
    }

#elif defined V_STRING
typedef std::string  test_val_t;
typedef test_val_t * val_sink_t;

#if V_LEN == 0
#define V_LEN 10
//static_assert( 0, "When using strings you must specify \"V_LEN=<Length of val string\"");
#endif
#define VAL_LEN V_LEN
test_val_t
gen_val() {
    test_val_t new_val = "";
    for (uint32_t i = 0; i < VAL_LEN; i++) {
        new_val += rand() % 256;
    }
    return new_val;
}

#else
typedef uint32_t   test_val_t;
    typedef test_val_t val_sink_t;
    #define VAL_LEN sizeof(test_val_t)
    test_val_t
    gen_val() {
        return rand() * rand();
    }

//static_assert(0, "Make with either \"V=INT32\", \"V=INT64\", or " "\"V=STRING\". If Using \"V=STRING\" also be sure to set " "\"V_LEN=<desired length of string keys\"");
#endif

#ifndef TEST_LEN
static int TEST_LEN = 12345678;
#endif

#ifndef INIT_SIZE
static int INIT_SIZE = 4096;
#endif

#ifndef QUERY_RATE
static int QUERY_RATE = 1;
#endif

#ifndef REMOVE_RATE
static double REMOVE_RATE = 0.2;
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

static int64_t now2ms()
{
    auto tp = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
}

static void init_keys(std::vector<test_key_t> & insert_keys);
static void init_vals(std::vector<test_val_t> & insert_vals);
static void init_query_keys(std::vector<test_key_t> & insert_keys,
                            std::vector<test_key_t> & query_keys);
static void init_remove_keys(std::vector<test_key_t> & insert_keys,
                             std::vector<test_key_t> & remove_keys);


template<typename ht>
static void run_table(std::vector<test_key_t> & insert_keys,
                           std::vector<test_val_t> & insert_vals,
                           std::vector<test_key_t> & query_keys,
                           std::vector<test_key_t> & remove_keys);


static void report(double,
                   const char *      header);

static void clear_cache();

inline static uint32_t
rand_above_perc(int rd) {
	return rand() < rd;
}

int main(int argc, char* argv[])
{
	srand(time(0));
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
    }
#endif

    printf("if %.1lf rf %.1lf qf %.1lf rr %.1lf qr %d n %d\n", INSERT_FAILURE_RATE, REMOVE_FAILURE_RATE, QUERY_FAILURE_RATE, REMOVE_RATE, QUERY_RATE, TEST_LEN);

    init_keys(insert_keys);
    init_vals(insert_vals);
    init_query_keys(insert_keys, query_keys);
    init_remove_keys(insert_keys, remove_keys);

#if __linux__ && __x86_64__
    { run_table<fht_table<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
#endif

    { run_table<emhash6::HashMap<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<ska::flat_hash_map<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<emhash5::HashMap<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<robin_hood::unordered_flat_map<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<emhash7::HashMap<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<phmap::flat_hash_map<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }
    { run_table<tsl::robin_map<test_key_t, test_val_t>>(insert_keys, insert_vals, query_keys, remove_keys); }

    return 0;
}

static void
init_keys(std::vector<test_key_t> & insert_keys) {
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        // duplicates here mean nothing...
        insert_keys.push_back(gen_key());
    }
}

static void
init_vals(std::vector<test_val_t> & insert_vals) {
	uint32_t insert_fail = INSERT_FAILURE_RATE * RAND_MAX;
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        if (rand_above_perc(insert_fail) || (!i)) {
            insert_vals.push_back(gen_val());
        }
        else {
            insert_vals.push_back(insert_vals[rand() % i]);
        }
    }
}

static void
init_query_keys(std::vector<test_key_t> & insert_keys,
                std::vector<test_key_t> & query_keys) {
	uint32_t query_fail = QUERY_FAILURE_RATE * RAND_MAX;
    for (uint32_t i = 0; i < TEST_LEN * QUERY_RATE; i++) {
        if (rand_above_perc(query_fail)) {
            query_keys.push_back(
                insert_keys[(rand() % (i + (i == 0))) % TEST_LEN]);
        }
        else {
            // assuming randomly generated key will not be found
            query_keys.push_back(gen_key());
        }
    }
}
static void
init_remove_keys(std::vector<test_key_t> & insert_keys,
                 std::vector<test_key_t> & remove_keys) {
    uint32_t upper_bound = (TEST_LEN * REMOVE_RATE) + 1;
	uint32_t remove_fail = REMOVE_FAILURE_RATE * RAND_MAX;
    for (uint32_t i = 0; i < upper_bound; i++) {
        if (rand_above_perc(remove_fail)) {
            remove_keys.push_back(
                insert_keys[(rand() % (i + (i == 0))) % TEST_LEN]);
        }
        else {
            // assuming randomly generated key will not be found
            remove_keys.push_back(gen_key());
        }
    }
}

static void
clear_cache() {

#if __linux__ && __x86_64__
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
            ptrs[i][j] = rand();
        }
    }
    for (uint32_t i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < clear_size; j++) {
            ptrs[i][j] += rand();
        }
        assert(!munmap(ptrs[i], clear_size * sizeof(uint32_t)));
    }
#else


#endif
}

template<typename ht>
static void run_table(std::vector<test_key_t> & insert_keys,
               std::vector<test_val_t> & insert_vals,
               std::vector<test_key_t> & query_keys,
               std::vector<test_key_t> & remove_keys) {

    //clear_cache();
    ht test_table(INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    auto start_time = now2ms();
    auto sum = 0;
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        test_table[insert_keys[i]] = insert_vals[i];
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            sum += test_table.count(query_keys[j]);
        }
        if (i == next_remove) {
            volatile auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
            sum += sink;
        }
    }
    auto end_time = now2ms();

    auto map_name = std::string(typeid(ht).name());
    if (map_name.find("Im") != std::string::npos)
        map_name = map_name.substr(2, map_name.find("Im") - 2);
    if (map_name.find("Ij") != std::string::npos)
        map_name = map_name.substr(2, map_name.find("Ij") - 2);

    report(end_time - start_time, map_name.data());
//    printf("sum = %d lf = %.2f\n", sum, test_table.load_factor());
}

static void
report(double ns_diff, const char * header) {

    const uint32_t total_ops =
        (uint32_t)(TEST_LEN * (1 + QUERY_RATE + REMOVE_RATE));
    static int once = 0;
    fprintf(stderr, "%s Perf -> \n", header);
    if (once++ == 0) {
    fprintf(stderr, "\tTotal Operations: %d\n", total_ops);
    fprintf(stderr,
            "\t\tInserts (%d), Failure Rate (%.3f)\n",
            TEST_LEN,
            (float)INSERT_FAILURE_RATE);
    fprintf(stderr,
            "\t\tQuerys  (%d), Failure Rate (%.3f)\n",
            QUERY_RATE * TEST_LEN,
            (float)QUERY_FAILURE_RATE);
    fprintf(stderr,
            "\t\tRemoves (%d), Failure Rate (%.3f)\n",
            (uint32_t)(REMOVE_RATE * TEST_LEN),
            (float)REMOVE_FAILURE_RATE);
    }

    const uint64_t ns_mult = 1000 * 1000 * 1000;

//    fprintf(stderr, "\t%.3lf Sec\n", ns_diff / ns_mult);
//    fprintf(stderr, "\t%.3lf US\n", ns_diff / (ns_mult / (1000 * 1000)));
    fprintf(stderr, "\t%.3lf MS ", ns_diff / (ns_mult / 1000));
    fprintf(stderr, "\t%.3lf NS -> %.3lf ns / op\n\n", ns_diff, ns_diff / total_ops);
}
