#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <vector>

#include "ska/flat_hash_map.hpp"
#include "fht/fht_ht.hpp"

#include "hash_table5.hpp"
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
#ifdef K_INT32
typedef uint32_t test_key_t;
#define KEY_LEN sizeof(test_key_t)
test_key_t
gen_key() {
    return random();
}

#elif defined K_INT64
typedef uint64_t test_key_t;
#define KEY_LEN sizeof(test_key_t)
test_key_t
gen_key() {
    return random() * random();
}

#elif defined K_STRING
typedef std::string test_key_t;
#if K_LEN == 0
static_assert(
    0,
    "When using strings you must specify \"K_LEN=<Length of key string\"");
#endif
#define KEY_LEN K_LEN
test_key_t
gen_key() {
    test_key_t new_key = "";
    for (uint32_t i = 0; i < KEY_LEN; i++) {
        new_key += random() % 256;
    }
    return new_key;
}

#else
static_assert(
    0,
    "Make with either \"K=INT32\", \"K=INT64\", or \"K=STRING\". If Using "
    "\"K=STRING\" also be sure to set \"K_LEN=<desired length of string "
    "keys\"");
#endif

#ifdef V_INT32
typedef uint32_t   test_val_t;
typedef test_val_t val_sink_t;
#define VAL_LEN sizeof(test_val_t)
test_val_t
gen_val() {
    return random() * random();
}

#elif defined V_INT64
typedef uint64_t   test_val_t;
typedef test_val_t val_sink_t;
#define VAL_LEN sizeof(test_val_t)
test_val_t
gen_val() {
    return random() * random();
}

#elif defined V_STRING
typedef std::string  test_val_t;
typedef test_val_t * val_sink_t;

#if V_LEN == 0
static_assert(
    0,
    "When using strings you must specify \"V_LEN=<Length of val string\"");
#endif
#define VAL_LEN V_LEN
test_val_t
gen_val() {
    test_val_t new_val = "";
    for (uint32_t i = 0; i < VAL_LEN; i++) {
        new_val += random() % 256;
    }
    return new_val;
}

#else
static_assert(0,
              "Make with either \"V=INT32\", \"V=INT64\", or "
              "\"V=STRING\". If Using \"V=STRING\" also be sure to set "
              "\"V_LEN=<desired length of string keys\"");
#endif

#ifndef TEST_LEN
static_assert(0, "Make with \"LEN=<Num items to test>\"");
#endif

#ifndef INIT_SIZE
#define INIT_SIZE 4096
#error "Using init size = 4096. To set Make with \"ISIZE=<(int) init size>\""
#endif

#ifndef QUERY_RATE
#define QUERY_RATE 0
#error                                                                         \
    "Query Rate not specified. Defaulting to 0. To set query rate make with \"QR=<(int) queries per insert>\""
#endif

#ifndef REMOVE_RATE
#define REMOVE_RATE 0
#error                                                                         \
    "Remove Rate not specified. Defaulting to 0. To set remove rate make with \"RR=<(float) removes per insert>\""
#endif

#ifndef INSERT_FAILURE_RATE
#error                                                                         \
    "Insert Failure Rate not specified. Defaulting to 0. To set make with \"IFR=<(float) %% of inserts to be unique>\""
#endif

#ifndef QUERY_FAILURE_RATE
#error                                                                         \
    "Query Failure Rate not specified. Defaulting to 0. To set make with \"QFR=<(float) %% of querys to have valid key>\""
#endif

#ifndef REMOVE_FAILURE_RATE
#error                                                                         \
    "Remove Failure Rate not specified. Defaulting to 0. To set make with \"RFR=<(float) %% of removes to have valid key>\""
#endif

static void init_keys(std::vector<test_key_t> & insert_keys);
static void init_vals(std::vector<test_val_t> & insert_vals);
static void init_query_keys(std::vector<test_key_t> & insert_keys,
                            std::vector<test_key_t> & query_keys);
static void init_remove_keys(std::vector<test_key_t> & insert_keys,
                             std::vector<test_key_t> & remove_keys);

static void run_emb5(std::vector<test_key_t> & insert_keys,
                     std::vector<test_val_t> & insert_vals,
                     std::vector<test_key_t> & query_keys,
                     std::vector<test_key_t> & remove_keys);

static void run_emb6(std::vector<test_key_t> & insert_keys,
                     std::vector<test_val_t> & insert_vals,
                     std::vector<test_key_t> & query_keys,
                     std::vector<test_key_t> & remove_keys);

static void run_my_table(std::vector<test_key_t> & insert_keys,
                         std::vector<test_val_t> & insert_vals,
                         std::vector<test_key_t> & query_keys,
                         std::vector<test_key_t> & remove_keys);

static void run_flat_table(std::vector<test_key_t> & insert_keys,
                           std::vector<test_val_t> & insert_vals,
                           std::vector<test_key_t> & query_keys,
                           std::vector<test_key_t> & remove_keys);

static void report(struct timespec * start,
                   struct timespec * end,
                   const char *      header);

static void clear_cache();

static uint32_t
rand_above_perc(float desire_percent) {
    float r = random();
    r /= RAND_MAX;

    // >= so that 0.0 never returns false
    return (r >= desire_percent);
}

int
main() {
    std::vector<test_key_t> insert_keys;
    std::vector<test_val_t> insert_vals;
    std::vector<test_key_t> query_keys;
    std::vector<test_key_t> remove_keys;

    init_keys(insert_keys);
    init_vals(insert_vals);
    init_query_keys(insert_keys, query_keys);
    init_remove_keys(insert_keys, remove_keys);

    clear_cache();
    run_my_table(insert_keys, insert_vals, query_keys, remove_keys);
    clear_cache();
    run_flat_table(insert_keys, insert_vals, query_keys, remove_keys);
    clear_cache();
    run_emb5(insert_keys, insert_vals, query_keys, remove_keys);
    clear_cache();
    run_emb6(insert_keys, insert_vals, query_keys, remove_keys);
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
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        if (rand_above_perc((float)INSERT_FAILURE_RATE) || (!i)) {
            insert_vals.push_back(gen_val());
        }
        else {
            insert_vals.push_back(insert_vals[random() % i]);
        }
    }
}

static void
init_query_keys(std::vector<test_key_t> & insert_keys,
                std::vector<test_key_t> & query_keys) {
    for (uint32_t i = 0; i < TEST_LEN * QUERY_RATE; i++) {
        if (rand_above_perc((float)QUERY_FAILURE_RATE)) {
            query_keys.push_back(
                insert_keys[(random() % (i + (i == 0))) % TEST_LEN]);
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
    for (uint32_t i = 0; i < upper_bound; i++) {
        if (rand_above_perc((float)REMOVE_FAILURE_RATE)) {
            remove_keys.push_back(
                insert_keys[(random() % (i + (i == 0))) % TEST_LEN]);
        }
        else {
            // assuming randomly generated key will not be found
            remove_keys.push_back(gen_key());
        }
    }
}

static void
clear_cache() {

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
            ptrs[i][j] = random();
        }
    }
    for (uint32_t i = 0; i < 10; i++) {
        for (uint32_t j = 0; j < clear_size; j++) {
            ptrs[i][j] += random();
        }
        assert(!munmap(ptrs[i], clear_size * sizeof(uint32_t)));
    }

    sleep(5);
}

static void
run_my_table(std::vector<test_key_t> & insert_keys,
             std::vector<test_val_t> & insert_vals,
             std::vector<test_key_t> & query_keys,
             std::vector<test_key_t> & remove_keys) {

    struct timespec                   start_time, end_time;
    fht_table<test_key_t, test_val_t> test_table(INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    val_sink_t vsink;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        test_table.emplace(insert_keys[i], insert_vals[i]);
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            volatile auto sink = test_table.find(query_keys[j]);
        }
        if (i == next_remove) {
            volatile auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    report(&start_time, &end_time, "My Hashtable");
}

static void
run_emb5(std::vector<test_key_t> & insert_keys,
         std::vector<test_val_t> & insert_vals,
         std::vector<test_key_t> & query_keys,
         std::vector<test_key_t> & remove_keys) {

    struct timespec                          start_time, end_time;
    emhash5::HashMap<test_key_t, test_val_t> test_table(INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    val_sink_t vsink;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        test_table.emplace(insert_keys[i], insert_vals[i]);
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            volatile auto sink = test_table.find(query_keys[j]);
        }
        if (i == next_remove) {
            volatile auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    report(&start_time, &end_time, "emhash5");
}


static void
run_emb6(std::vector<test_key_t> & insert_keys,
         std::vector<test_val_t> & insert_vals,
         std::vector<test_key_t> & query_keys,
         std::vector<test_key_t> & remove_keys) {

    struct timespec                          start_time, end_time;
    emhash6::HashMap<test_key_t, test_val_t> test_table(INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    val_sink_t vsink;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        test_table.emplace(insert_keys[i], insert_vals[i]);
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            volatile auto sink = test_table.find(query_keys[j]);
        }
        if (i == next_remove) {
            volatile auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    report(&start_time, &end_time, "emhash6");
}

static void
run_flat_table(std::vector<test_key_t> & insert_keys,
               std::vector<test_val_t> & insert_vals,
               std::vector<test_key_t> & query_keys,
               std::vector<test_key_t> & remove_keys) {

    struct timespec                            start_time, end_time;
    ska::flat_hash_map<test_key_t, test_val_t> test_table(INIT_SIZE);

    uint32_t next_remove, remove_iter = 0;
    next_remove =
        REMOVE_RATE != 0 ? (uint32_t)(1.0 / ((float)REMOVE_RATE)) : TEST_LEN;

    const uint32_t remove_incr = next_remove;

    clock_gettime(CLOCK_MONOTONIC, &start_time);
    for (uint32_t i = 0; i < TEST_LEN; i++) {
        test_table[insert_keys[i]] = insert_vals[i];
        for (uint32_t j = i * QUERY_RATE; j < (i + 1) * QUERY_RATE; j++) {
            volatile auto sink = test_table.find(query_keys[j]);
        }
        if (i == next_remove) {
            volatile auto sink = test_table.erase(remove_keys[remove_iter++]);
            next_remove += remove_incr;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    report(&start_time, &end_time, "Flat Hashtable");
}

static void
report(struct timespec * start, struct timespec * end, const char * header) {

    const uint32_t total_ops =
        (uint32_t)(TEST_LEN * (1 + QUERY_RATE + REMOVE_RATE));
    fprintf(stderr, "%s Perf -> \n", header);
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

    const uint64_t ns_mult = 1000 * 1000 * 1000;

    double ns_start = start->tv_sec * ns_mult + start->tv_nsec;
    double ns_end   = end->tv_sec * ns_mult + end->tv_nsec;
    double ns_diff  = ns_end - ns_start;

    fprintf(stderr, "\t%.3lf Sec\n", ns_diff / ns_mult);
    fprintf(stderr, "\t%.3lf MS\n", ns_diff / (ns_mult / 1000));
    fprintf(stderr, "\t%.3lf US\n", ns_diff / (ns_mult / (1000 * 1000)));
    fprintf(stderr,
            "\t%.3lf NS -> %.3lf ns / op\n",
            ns_diff,
            ns_diff / total_ops);
    fprintf(stderr, "\n");
}
