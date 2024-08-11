#include "util.h"

#include "martin/robin_hood.h"
#include "tsl/robin_map.h"
#include "tsl/hopscotch_map.h"
#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"
#include "phmap/phmap.h"
#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
//#include "patchmap/patchmap.hpp"


#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_table5.hpp"
//#include "old/hash_table2.hpp"

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
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
    Timer(const char* msg, const char* msg2) :_msg(msg) { clock_gettime(CLOCK_MONOTONIC, &_start); }
    ~Timer()
    {
        struct timespec end;
        clock_gettime(CLOCK_MONOTONIC, &end);

        //milliseconds
        double msec = double((end.tv_sec - _start.tv_sec) * 1000ULL) + double(end.tv_nsec - _start.tv_nsec)*0.000001;
        if (msec < 10000)
            printf("%12s: %u ms\n", _msg, (unsigned)msec);
        else
            printf("%12s: %.2lf sec\n", _msg, msec / 1000.0);
    }
private:
    const char* _msg;
    struct timespec _start;
};
#endif //_WIN32

//typedef std::string Value;
#if TVal == 0
using Value = uint32_t;
#elif TVal == 1
using Value = uint64_t;
#else
using Value = std::string;
#endif

static const uint64_t MAX_ELEMENTS = 300'0000;
static const uint32_t LOOPS = 30;
static uint64_t* ELEMENTS;


static inline Value make_value(uint64_t v, const Value*) {
#if TVal < 2
    return (Value)v;
#else
    return "0";
#endif
}

template<class T>
static void iterator(T& m, const char* msg = nullptr)
{
    Timer t("iterator", msg);
    for (int n = LOOPS; n--;) {
        int sum = 0;
        for(auto v: m) sum += v.first;
        m[sum] = make_value(sum, 0);
    }
}

template<class T>
static void insert_operator(T& m, const char* msg = nullptr)
{
    Timer t("insert[]", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    const Value v = make_value(1, (const Value*)0);
    for (int n = LOOPS; n--;)
    for (const uint64_t* p = ELEMENTS; p != end; ++p)
        m[*p] = v;
}

template<class T>
static void insert(T& m, const char* msg = nullptr)
{
    Timer t("insert_pair", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    for (int n = LOOPS; n--;)
    for (const uint64_t* p = ELEMENTS; p != end; ++p)
        if (!m.count(*p))
        m.emplace(*p, make_value(*p, (const Value*)0));
}

template<class T>
static void insert_assign(T& m, const char* msg = nullptr)
{
    Timer t("insert_assign", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    for (int n = LOOPS; n--;)
        for (const uint64_t* p = ELEMENTS; p != end; ++p) {
            Value v = make_value(*p, (const Value*)0);
            m.insert_or_assign(*p, std::move(v));
        }
}

template<class T>
static void emplace(T& m, const char* msg = nullptr)
{
    Timer t("emplace", msg);

    const uint64_t* end = ELEMENTS + MAX_ELEMENTS;
    const Value v = make_value(1, (const Value*)0);
    for (int n = LOOPS; n--;)
    for (const uint64_t* p = ELEMENTS; p != end; ++p)
        m.emplace(*p, v);
}

template<class T>
static void erase(T& m, const char* msg = nullptr)
{
    Timer t("erase", msg);

    for (int n = LOOPS; n--;)
    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p)
        m.erase(*p);
}

template<class T>
static void find_erase(T& m, const char* msg = nullptr)
{
    Timer t("find_erase", msg);

    for (int n = LOOPS; n--;)
    for (const uint64_t* p = ELEMENTS, *end = ELEMENTS + MAX_ELEMENTS; p != end; ++p) {
        if (m.count(*p))
            m.erase(*p);
    }
}

template<class T>
static uint64_t find(const T& m, const char* msg = nullptr)
{
    uint64_t ret = 0;

    Timer t("find", msg);

    for (int n = LOOPS; n--;)
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

    for (int n = LOOPS; n--;)
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

    for (int n = LOOPS; n--;) {
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
    tmp.max_load_factor(0.88f);
    for (int n = LOOPS; n--;) {
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

    for (int n = 100000; n--;)
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
    for (int n = LOOPS; n--;) {
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
    long ret = 0;
    Timer t(name, "bench");

    puts(name);
    //TEST(m);
    m.max_load_factor(0.88f);
    //insert_assign(m);
    insert_operator(m);
    printf("load_factor = %.2lf\n", m.load_factor());

    erase(m);
    m.clear();

    insert_operator(m);
    find_erase(m);

    insert_operator(m);
    m.clear();

    emplace(m, "emplace");
    m.clear();

    insert_operator(m);
    ret = find(m);
    ret += count(m);

    ctor_initlist(m);
    m.clear();
    //ctor_iterators(m);

    insert(m);
    ret += copy_ctor(m);
    ret += copy_operator(m);
    iterator(m);
    printf("\nload_factor = %.2lf %ld", m.load_factor(), ret);
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
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;

        uint64_t offset = time(0);
        for (uint64_t i = 0; i != MAX_ELEMENTS; ++i) {
            offset = rnd(i + offset);
            //ELEMENTS[i] = offset;
            ELEMENTS[i] = dis(gen);
        }
    }

#ifdef HOOD_HASH
    using hash_t = robin_hood::hash<int64_t>;
#elif ABSL_HASH
    using hash_t = absl::Hash<int64_t>;
#elif FIB_HASH
    using hash_t = Int64Hasher<int64_t>;
#else
    using hash_t = std::hash<int64_t>;
#endif

    uint64_t ret = 0;
#if ABSL_HMAP
    { absl::flat_hash_map<uint64_t, Value, hash_t> m6; ret -= test(m6, "\nabsl::flat_hash_map"); }
#endif
#if QC_HASH
    { qc::hash::RawMap<uint64_t, Value, hash_t> m6; ret -= test(m6, "\nqc::hash::map"); }
//    { fph::DynamicFphMap<uint64_t, Value, fph::MixSeedHash<uint64_t>> m6; ret -= test(m6, "\nfph::FphMap"); }
#endif
    { robin_hood::unordered_flat_map<uint64_t, Value, hash_t> m4; ret -= test(m4, "\nrobin_hood::unordered_flat_map"); }
    { emilib::HashMap<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nemilib::HashMap"); }
    { emilib2::HashMap<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nemilib2::HashMap"); }
#if HAVE_BOOST
    { boost::unordered_flat_map<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nboost::unordered_flat_map"); }
#endif
    { robin_hood::unordered_node_map<uint64_t, Value, hash_t> m4; ret -= test(m4, "\nrobin_hood::unordered_node_map"); }
//    { emhash4::HashMap<uint64_t, Value, hash_t> m7; ret -= test(m7, "\nemhash4::HashMap"); }
//    { emilib3::HashMap<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nemilib3::HashMap"); }
//    { hrd7::hash_map<uint64_t, Value, hash_t> m1; ret -= test(m1, "\nhrd::hash_map"); }
    { emhash5::HashMap<uint64_t, Value, hash_t> m5; ret -= test(m5, "\nemhash5::HashMap"); }
    { tsl::robin_map<uint64_t, Value, hash_t> m0; ret -= test(m0, "\ntsl::robin_map"); }
    { tsl::hopscotch_map<uint64_t, Value, hash_t> m0; ret -= test(m0, "\ntsl::hopscotch_map"); }
    { ska::flat_hash_map<uint64_t, Value, hash_t> m0; ret -= test(m0, "\nska::flat_hash_map"); }
    { ska::bytell_hash_map<uint64_t, Value, hash_t> m0; ret -= test(m0, "\nska::bytell_hash_map"); }
 //   { emhash2::HashMap<uint64_t, Value, hash_t> m2; ret -= test(m2, "\nemhash2::HashMap"); }
    { emhash7::HashMap<uint64_t, Value, hash_t> m6; ret -= test(m6, "\nemhash7::HashMap"); }

    //google::dense_hash_map<uint64_t, Value, hash_t> m2;ret -= test(m2, "\ngoogle::dense_hash_map");
    { phmap::flat_hash_map<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nparallel-hashmap::flat_map"); }
    { phmap::node_hash_map<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nparallel-hashmap::node_map"); }
    { std::unordered_map<uint64_t, Value, hash_t> m3; ret -= test(m3, "\nstd::unordered_map"); }
//    { whash::patchmap<uint64_t, Value, hash_t> m8; ret -= test(m8, "\nwhash::patchmap"); } //insert_or_assign is not exist

    delete[] ELEMENTS;
    return (int)ret;
}
