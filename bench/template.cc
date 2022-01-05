#ifndef _WIN32
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>
#include <iostream>
#include <chrono>
#include <array>

#if EMH == 7
#include "emhash7_hash_map.cc"
#elif EMH == 5
#include "emhash5_hash_map.cc"
#elif EMH == 1
#include "emilib_hash_map.cc"
#elif EMH == 2
#include "emilib_hash_map.cc"
#endif

#include "hash_map_int_base.h"
#include "hash_map_str_base.h"


// TODO When generating random values to insert in the map there is no check
// to see if duplicate random values are generated. Could improve that (but the probability is so so
// low and the impact nearly null that it's not really worth it).

static const std::array<char, 62> ALPHANUMERIC_CHARS = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
};

/**
 * SMALL_STRING_SIZE should be small enough so that there is no heap allocation when a std::string is created.
 */
static const std::size_t SMALL_STRING_MIN_SIZE = 10;
static const std::size_t SMALL_STRING_MAX_SIZE = 20;
static const std::size_t STRING_MIN_SIZE = 20;
static const std::size_t STRING_MAX_SIZE = 100;

static const std::int64_t SEED = 2022;
static std::mt19937_64 generator(SEED);
static std::int64_t num_keys;
static std::string test_type;
static std::int64_t value;

#ifndef ITERATE_INT
#define ITERATE_INT(it) for(const auto& it : hash)
#endif

#ifndef SHUFFLE_STR_ARRAY
#define SHUFFLE_STR_ARRAY(keys) std::shuffle(keys.begin(), keys.end(), generator)
#endif

std::size_t get_memory_usage_bytes() {
#ifndef _WIN32
    std::ifstream file("/proc/self/statm");

    std::size_t memory;
    file >> memory; // Ignore first
    file >> memory;
    return memory * getpagesize();
#else
    return 0;
#endif
}

std::string get_random_alphanum_string(
    std::size_t min_size, std::size_t max_size)
{
    std::uniform_int_distribution<std::size_t> rd_uniform_size(min_size, max_size);
    size_t size = rd_uniform_size(generator);

    std::uniform_int_distribution<std::size_t> rd_uniform(0, ALPHANUMERIC_CHARS.size() - 1);

    std::string str(size, '\0');
    for(std::size_t i = 0; i < size; i++) {
        str[i] = ALPHANUMERIC_CHARS[rd_uniform(generator)];
    }

    return str;
}

/**
 * Generate a vector [0, nb_ints) and shuffle it
 */
std::vector<std::int64_t> get_random_shuffle_range_ints(std::size_t nb_ints) {
    std::vector<std::int64_t> random_shuffle_ints(nb_ints);
    int num = 0;
    for (auto& v : random_shuffle_ints)
        v = num++;
//    std::iota(random_shuffle_ints.begin(), random_shuffle_ints.end(), 0);
    std::shuffle(random_shuffle_ints.begin(), random_shuffle_ints.end(), generator);

    return random_shuffle_ints;
}

/**
 * Generate random vector of random ints between min and max.
 */
std::vector<std::int64_t> get_random_full_ints(
    std::size_t nb_ints,
    std::int64_t min = 0,
    std::int64_t max = std::numeric_limits<std::int64_t>::max())
{
    std::uniform_int_distribution<std::int64_t> rd_uniform(min, max);

    std::vector<std::int64_t> random_ints(nb_ints);
    for(std::size_t i = 0; i < random_ints.size(); i++) {
        random_ints[i] = rd_uniform(generator);
    }

    return random_ints;
}


std::vector<std::string> get_random_alphanum_strings(
    std::size_t nb_strings, std::size_t min_size, std::size_t max_size)
{
    std::vector<std::string> random_strings(nb_strings);
    for(std::size_t i = 0; i < random_strings.size(); i++) {
        random_strings[i] = get_random_alphanum_string(min_size, max_size);
    }

    return random_strings;
}

class measurements {
 public:
 measurements(): m_memory_usage_bytes_start(get_memory_usage_bytes()),
        m_chrono_start(std::chrono::high_resolution_clock::now())

    {
    }

    void set_chrono_start()
    {
        m_chrono_start = std::chrono::high_resolution_clock::now();
    }

    ~measurements() {
        const auto chrono_end = std::chrono::high_resolution_clock::now();
        const std::size_t memory_usage_bytes_end = get_memory_usage_bytes();

        const double nb_seconds = std::chrono::duration<double>(chrono_end - m_chrono_start).count();
        // On reads or delete the used bytes could be less than initially.
        const std::size_t used_memory_bytes = (memory_usage_bytes_end > m_memory_usage_bytes_start)?
            memory_usage_bytes_end - m_memory_usage_bytes_start:0;

        std::cout << test_type << "\n\t\t" << nb_seconds*1000 << " ms|" << used_memory_bytes/1024 << " kb ";
    }

 private:
    std::size_t m_memory_usage_bytes_start;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_chrono_start;
};

static bool process_integers()
{
    bool ret = true;

#ifdef SETUP_INT
    SETUP_INT;

    /**
     * Integers
     */
    std::vector<std::int64_t> keys;

    if (test_type == "insert_random_shuffle_range" ||
        test_type == "reinsert_random_shuffle_range" ||
        test_type == "read_random_shuffle_range" ||
        test_type == "read_miss_random_shuffle_range" ||
        test_type == "insert_random_shuffle_range_reserve" ||
        test_type == "read_random_shuffle_range_after_delete" ||
        test_type == "iteration_random_shuffle_range" ||
        test_type == "delete_random_shuffle_range")
    {
        if (test_type == "insert_random_shuffle_range_reserve"){
            RESERVE_INT(num_keys);
        }
        keys = get_random_shuffle_range_ints(num_keys);
    }else if (test_type == "insert_random_full" ||
              test_type == "reinsert_random_full" ||
              test_type == "read_random_full" ||
              test_type == "read_miss_random_full" ||
              test_type == "insert_random_full_reserve" ||
              test_type == "read_random_full_after_delete" ||
              test_type == "iteration_random_full" ||
              test_type == "delete_random_full")
    {
        if (test_type == "insert_random_full_reserve"){
            RESERVE_INT(num_keys);
        }
        keys = get_random_full_ints(num_keys);
    }

    if (test_type == "insert_random_shuffle_range" ||
        test_type == "insert_random_full")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }
    }

    else if (test_type == "insert_random_shuffle_range_reserve" ||
             test_type == "insert_random_full_reserve")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }
    }

    else if (test_type == "reinsert_random_shuffle_range" ||
             test_type == "reinsert_random_full")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }
        std::shuffle(keys.begin(), keys.end(), generator);


        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }
    }

    else if (test_type == "read_random_shuffle_range" ||
             test_type == "read_random_full")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }

        std::shuffle(keys.begin(), keys.end(), generator);

        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_INT_EXISTING(keys[i]);
        }
    }

    else if (test_type == "read_miss_random_shuffle_range" ||
             test_type == "read_miss_random_full")
    {
        std::vector<std::int64_t> keys_read;
        if (test_type == "read_miss_random_shuffle_range"){
            keys_read =
                get_random_full_ints(num_keys, std::numeric_limits<std::int64_t>::min(), -3);
        }else if (test_type == "read_miss_random_full"){
            keys_read =
                get_random_full_ints(num_keys, std::numeric_limits<std::int64_t>::min(), -3);
        }

        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }


        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_INT_MISSING(keys_read[i]);
        }
    }

    else if (test_type == "read_random_shuffle_range_after_delete" ||
             test_type == "read_random_full_after_delete")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }

        std::shuffle(keys.begin(), keys.end(), generator);
        for(std::int64_t i = 0; i < num_keys / 2; i++) {
            DELETE_INT(keys[i]);
        }

        std::shuffle(keys.begin(), keys.end(), generator);


        std::int64_t nb_found = 0;
        m.set_chrono_start();
        std::string s_val; // for kyotocabinet_stash
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_INT_EXISTING_COUNT(keys[i], nb_found);
        }

        if(nb_found != num_keys / 2) {
            std::cerr << "error, duplicates" << nb_found << " != " << num_keys / 2;
            std::exit(6);
        }
    }

    else if (test_type == "iteration_random_shuffle_range" ||
             test_type == "iteration_random_full")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }


        m.set_chrono_start();
        ITERATE_INT(it) {
            CHECK_INT_ITERATOR_VALUE(it, value);
        }
    }

    else if (test_type == "delete_random_shuffle_range" ||
             test_type == "delete_random_full")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_INT(keys[i], value);
        }

        std::shuffle(keys.begin(), keys.end(), generator);


        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            DELETE_INT(keys[i]);
        }
    }

    else {
        ret = false;
    }

    if (ret) {
        float load_factor_int = 0;
        load_factor_int = LOAD_FACTOR_INT_HASH(hash);
        std::cout << load_factor_int << std::endl;
    }

    CLEAR_INT;
#else
    ret = false;
#endif

    return ret;
}

static bool process_strings()
{
    bool ret = true;
#ifdef SETUP_STR
    SETUP_STR;

    /**
     * Small strings
     */
    std::vector<std::string> keys;

    if (test_type == "insert_small_string" ||
        test_type == "reinsert_small_string" ||
        test_type == "insert_small_string_reserve" ||
        test_type == "read_small_string" ||
        test_type == "read_miss_small_string" ||
        test_type == "read_small_string_after_delete" ||
        test_type == "delete_small_string")
    {
        if (test_type == "read_miss_small_string"){
            RESERVE_STR(num_keys);
        }
        keys = get_random_alphanum_strings(
            num_keys, SMALL_STRING_MIN_SIZE, SMALL_STRING_MAX_SIZE);
    }else if (test_type == "insert_string" ||
              test_type == "reinsert_string" ||
              test_type == "insert_string_reserve" ||
              test_type == "read_string" ||
              test_type == "read_miss_string" ||
              test_type == "read_string_after_delete" ||
              test_type == "delete_string")
    {
        if (test_type == "read_miss_string"){
            RESERVE_STR(num_keys);
        }
        keys = get_random_alphanum_strings(
            num_keys, STRING_MIN_SIZE, STRING_MAX_SIZE);
    }

    if (test_type == "insert_small_string" ||
        test_type == "insert_string" ||
        test_type == "insert_small_string_reserve" ||
        test_type == "insert_string_reserve")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }
    }

    else if (test_type == "reinsert_small_string" || test_type == "reinsert_string"){
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }
        SHUFFLE_STR_ARRAY(keys);


        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }
    }

    else if (test_type == "read_small_string" || test_type == "read_string"){
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], 1);
        }

        SHUFFLE_STR_ARRAY(keys);

        std::string s_val; // for leveldb
        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_STR_EXISTING(keys[i]);
        }
    }

    else if (test_type == "read_miss_small_string" || test_type == "read_miss_string"){
        std::vector<std::string> keys_read;
        if (test_type == "read_miss_string")
            keys_read = get_random_alphanum_strings(
                num_keys, STRING_MIN_SIZE, STRING_MAX_SIZE);
        else if (test_type == "read_miss_small_string")
            keys_read = get_random_alphanum_strings(
                num_keys, SMALL_STRING_MIN_SIZE, SMALL_STRING_MAX_SIZE);

        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }


        std::string s_val; // for kyotocabinet_stash
        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_STR_MISSING(keys_read[i]);
        }
    }

    else if (test_type == "read_small_string_after_delete" ||
             test_type == "read_string_after_delete")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }

        SHUFFLE_STR_ARRAY(keys);
        for(std::int64_t i = 0; i < num_keys / 2; i++) {
            DELETE_STR(keys[i]);
        }
        SHUFFLE_STR_ARRAY(keys);


        std::int64_t nb_found = 0;
        std::string s_val; // for kyotocabinet_stash
        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            FIND_STR_EXISTING_COUNT(keys[i], nb_found);
        }

        if(nb_found != num_keys / 2) {
            std::cerr << "error, duplicates" << nb_found << " != " << num_keys / 2;
            std::exit(6);
        }
    }

    else if (test_type == "delete_small_string" ||
             test_type == "delete_string")
    {
        measurements m;
        for(std::int64_t i = 0; i < num_keys; i++) {
            INSERT_STR(keys[i], value);
        }

        SHUFFLE_STR_ARRAY(keys);


        m.set_chrono_start();
        for(std::int64_t i = 0; i < num_keys; i++) {
            DELETE_STR(keys[i]);
        }
    }

    else {
        ret = false;
    }

    if (ret) {
        float load_factor_str = 0;
        load_factor_str = LOAD_FACTOR_STR_HASH(str_hash);
        std::cout << load_factor_str << std::endl;
    }

    CLEAR_STR;
#else
    ret = false;
#endif

    return ret;
}

int main(int argc, char ** argv) {
    num_keys = 1234567;
    if (argc > 1) {
        num_keys = std::stoll(argv[1]);
    }
    num_keys += num_keys & 1;

    std::cout << num_keys << " num_keys\n";
    std::vector<std::string> vlist = {
//    'random_shuffle_range':
        "read_random_shuffle_range",
        "insert_random_shuffle_range", "reinsert_random_shuffle_range",
 //   "random_full":
        "insert_random_full", "reinsert_random_full",
        "insert_random_full_reserve",
        "read_random_full", "read_miss_random_full",
        "delete_random_full", "read_random_full_after_delete",
        "iteration_random_full",

//    "small_string"
         "insert_small_string", "reinsert_small_string",
         "insert_small_string_reserve",
         "read_small_string", "read_miss_small_string",
         "delete_small_string",
         "read_small_string_after_delete",

//    "string":
        "insert_string", "reinsert_string",
        "insert_string_reserve",
        "read_string", "read_miss_string",
        "delete_string",
        "read_string_after_delete",

    };

    value = 1;
    for (auto& v : vlist) {
        test_type = v;
        if (!process_integers() && !process_strings()){
            std::cerr << "Unknown test type: " << test_type << ".\n";
            // Do not change the exit status 71! It is used by bench.py
            std::exit(71);
        }
    }

    return 0;
}
