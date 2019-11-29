#include "hrd/hash_set.h"
#include "martin/robin_hood.h"
#include "tsl/robin_map.h"

#include "hash_table2.hpp"
#include "hash_table6.hpp"
#include "phmap/phmap.h"

#include <random>
#include <iostream>
#include <unordered_map>

#ifdef _WIN32
#  include <Windows.h>
#else
#  include <time.h>
#endif

#ifdef _WIN32
class Timer
{
public:
    Timer(const char* msg, const char* msg2) :_msg(!msg2 ? msg : msg2), _start(GetTickCount()) {}
    ~Timer()
    {
        DWORD end = GetTickCount();
        printf("%14s: %u\n", _msg, (unsigned)(end - _start));
    }
private:
    const char* _msg;
    DWORD _start;
};
#else
class Timer
{
public:
    Timer(const char* msg, const char* msg2) :_msg(msg) { clock_gettime(CLOCK_MONOTONIC_RAW, &_start); }
    ~Timer()
    {
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC_RAW, &end);

        //milliseconds
        double msec = double((end.tv_sec - _start.tv_sec) * 1000ULL) + double(end.tv_nsec - _start.tv_nsec)*0.000001;
        
        printf("%12s: %u\n", _msg, (unsigned)msec);
    }
private:
    const char* _msg;
    struct timespec _start;
};
#endif //_WIN32

typedef uint64_t Key;
//typedef std::string Value;
typedef uint64_t Value;

const uint64_t MAX_ELEMENTS = 5000000;
uint64_t* ELEMENTS;


static HRD_ALWAYS_INLINE uint64_t make_value(uint64_t v, const Value*) {
    return v;
}

static HRD_ALWAYS_INLINE std::string make_value(uint64_t v, const std::string*) {
    return std::to_string(v);
}

template<class T>
static void insert_operator(T& m, const char* msg = nullptr)
{
    Timer t("insert[]", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    for (uint64_t n = 20; n--;)
        for (const uint64_t* p = ELEMENTS; p != end; ++p)
            m[*p] = make_value(*p, (const Value*)0);
}

template<class T>
static void insert(T& m, const char* msg = nullptr)
{
    Timer t("insert", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    for (uint64_t n = 20; n--;)
        for (const uint64_t* p = ELEMENTS; p != end; ++p)
            m.insert(T::value_type(*p, make_value(*p, (const Value*)0)));
}

template<class T>
static void emplace(T& m, const char* msg = nullptr)
{
    Timer t("emplace", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    for (uint64_t n = 20; n--;)
        for (const uint64_t* p = ELEMENTS; p != end; ++p)
            m.emplace(*p, make_value(*p,(const Value*)0));
}

template<class T>
static void erase(T& m, const char* msg = nullptr)
{
    Timer t("erase", msg);

    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p)
        m.erase(*p);
}

template<class T>
static void find_erase(T& m, const char* msg = nullptr)
{
    Timer t("find_erase", msg);

    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p) {
        auto it = m.find(*p);
        if (it != m.end())
            m.erase(it);
    }
}

template<class T>
static uint64_t find(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("find", msg);

    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p) {
        ret += (m.find(*p) != m.end());
    }

    return ret;
}

template<class T>
static uint64_t count(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("count", msg);

    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p) {
        ret += m.count(*p);
    }

    return ret;
}

template<class T>
static uint64_t copy_ctor(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("copy_ctor", msg);

    for (uint64_t n = 20; n--;) {
        T tmp(m);
        ret += tmp.size();
    }

    return ret;
}

template<class T>
static uint64_t copy_operator(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("copy_operator", msg);

    T tmp;
    for (uint64_t n = 20; n--;) {
        tmp = m;
        ret += tmp.size();
    }

    return ret;
}

#if 0
template<class T>
static uint64_t ctor_iterators(T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    std::vector<std::pair<T::key_type, T::mapped_type>> vv(MAX_ELEMENTS);
    for (size_t i = 0; i != MAX_ELEMENTS; ++i) {
        auto& r = vv[i];
        auto val = ELEMENTS[i];
        r.first = val;
        r.second = make_value(val, (const Value*)0);
    }

    Timer t("ctor_iterators", msg);

    for (uint64_t n = 10; n--;) {
        T tmp(vv.begin(), vv.end());
        ret += tmp.size();
    }

    m = T(vv.begin(), vv.end());

    return ret;
}
#endif

template<class T>
static uint64_t ctor_initlist(T& m, const char* msg = nullptr)
{
    Timer t("ctor_initlist", msg);

    m = T({ { 10,make_value(20,(const Value*)0) }, {20,make_value(30,(const Value*)0)}, { 11, make_value(20,(const Value*)0) }, { 21,make_value(30,(const Value*)0) } });

    return m.size();
}

template<class T>
static uint64_t TEST(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("TEST", msg);

    const size_t SIZE = 320 * 1024 * 1024;
    void* b = malloc(SIZE);
    for (uint64_t n = 20; n--;) {
        void* a = malloc(SIZE);
        memmove(a, b, SIZE);
        ret += *(uint64_t*)((char*)a + SIZE/2);
        free(a);
    }
    free(b);

    return ret;
}


template<class T>
static uint64_t test(T& m, const char* name)
{
    uint64_t ret = 0;
    puts(name);

    //TEST(m);
    //return 0;

/*
    m.clear();
    insert_operator(m);
    m.clear();
    emplace(m);

    m.clear();
    insert_operator(m);
    m.clear();
    emplace(m);
*/

    m.clear();
    insert_operator(m);
//    system("pause");
    erase(m);

    m.clear();
    //insert(m, "insert(clear)");
    insert_operator(m);
    find_erase(m);
    insert_operator(m);
    //insert(m, "insert(erase)"); // a lot of DELETED elements

    m.clear();
    emplace(m, "emplace(clear)");

    //insert(m, "insert(neg)");
    insert_operator(m);
    ret = find(m);
    ret += count(m);

    ctor_initlist(m);
    m.clear();
    //ctor_iterators(m);

    insert_operator(m);
    copy_ctor(m);
    copy_operator(m);

    m.clear();

    return ret;
}

static uint64_t xorshift(uint64_t n, uint64_t i) {
    return n ^ (n >> i);
}
static uint64_t rnd(uint64_t n) {
    uint64_t p = 0x5555555555555555; // pattern of alternating 0 and 1
    uint64_t c = 17316035218449499591ull;// random uneven integer constant; 
    return c * xorshift(p * xorshift(n, 32), 32);
}

int main()
{
    ELEMENTS = new uint64_t[MAX_ELEMENTS];
    
    //fill input data
    { 
/*
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;
*/
        uint64_t offset = 1;

        for (uint64_t i = 0; i != MAX_ELEMENTS; ++i) {
            offset = rnd(i + offset);
            ELEMENTS[i] = offset;
            //ELEMENTS[i] = dis(gen);
        }
    }

    //ska::flat_hash_map<uint64_t, Value> m0;
    hrd::hash_map<uint64_t, Value> m1;
//    hrd2::hash_map<uint64_t, Value> m12;
    //google::dense_hash_map<uint64_t, Value> m2;
    //m2.set_empty_key(0);
    //m2.set_deleted_key(-1);
    std::unordered_map<uint64_t, Value> m3;
    robin_hood::unordered_map<uint64_t, Value> m4;
    tsl::robin_map<uint64_t, Value> m5;
    emhash2::HashMap<uint64_t, Value> m7;
    emhash6::HashMap<uint64_t, Value> m6;
    phmap::flat_hash_map<uint64_t, Value> m8;

//    m7.max_load_factor(0.5f);
//    m6.max_load_factor(0.5f);

    const size_t ROUNDS = 1;
    uint64_t ret = 0;
    //ret -= test(m0, "ska::flat_hash_map");
    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m1, "\nhrd::hash_map");
    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m6, "\nemhash6::HashMap");
    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m7, "\nemhash7::HashMap");

    //ret -= test(m12, "\nhrd2::hash_map");
    //for (int i = 0; i != 3; ++i)
    //    ret -= test(m12, "\nhrd2::hash_map");
    //ret -= test(m2, "\ngoogle::dense_hash_map");
    //ret -= test(m3, "\nstd::unordered_map");
    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m4, "\nrobin_hood::unordered_map");

    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m5, "\ntsl::robin_map");

    for (size_t i = 0; i != ROUNDS; ++i)
        ret -= test(m8, "\nparallel-hashmap::flat_map");

    delete[] ELEMENTS;

    return (int)ret;
}
