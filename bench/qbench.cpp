#include <filesystem>
#include <fstream>
#include <iomanip>
#include <span>
#include "util.h"

#if QC_HASH
#include <qc-core/core.hpp>
#include "qchash/qc-hash.hpp"
#endif

#if CK_HMAP
#include "ck/Common/HashTable/HashMap.h"
#endif

#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#if X86_64
#include "hrd/hash_set_m.h"         //https://github.com/tessil/robin-map
#endif

#include "jg/dense_hash_map.hpp"
#include "rigtorp/rigtorp.hpp"


#include "hash_table5.hpp"
#include "hash_table6.hpp"
#include "hash_table7.hpp"
#include "hash_table8.hpp"

#include "emilib/emilib2s.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2ss.hpp"
//#include "emilib/emilib12.hpp"
//#include "emilib/emiset2.hpp"

#include "martin/robin_hood.h"
#include "ska/flat_hash_map.hpp"
#include "phmap/phmap.h"

#include "tsl/robin_map.h"
#include "tsl/robin_set.h"
#include "tsl/sparse_map.h"
#include "tsl/sparse_set.h"

#include "martin/unordered_dense.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#if CXX20
#include "fph/dynamic_fph_table.h"
#endif

#if FIB_HASH
    #define QintHasher Int64Hasher<K>
#elif HOOD_HASH
    #define QintHasher robin_hood::hash<K>
#elif ANKERL_HASH
    #define QintHasher ankerl::unordered_dense::hash<K>
//#elif QC_HASH
//    #define QintHasher typename qc::hash::RawMap<K, V>::hasher
#else
    #define QintHasher std::hash<K>
#endif

inline int64_t now()
{
    return std::chrono::nanoseconds(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template <typename C> concept IsMap = !std::is_same_v<typename C::mapped_type, void>;

static const std::vector<std::pair<size_t, size_t>> detailedElementRoundCountsRelease{
    {         5u, 200'000u},
    {        10u, 100'000u},
    {        25u,  40'000u},
    {        50u,  20'000u},
    {       100u,  10'000u},
    {       250u,   4'000u},
    {       500u,   2'000u},
    {     1'000u,   1'000u},
    {     2'500u,     400u},
    {     5'000u,     200u},
    {    10'000u,     100u},
    {    25'000u,      40u},
    {    50'000u,      20u},
    {   100'000u,      10u},
    {   250'000u,      10u},
    {   500'000u,      10u},
    { 1'000'000u,       5u},
    { 2'500'000u,       5u},
    { 5'000'000u,       5u},
    {10'000'000u,       3u}
};

static const std::vector<std::pair<size_t, size_t>> detailedElementRoundCountsDebug{
    {       10u, 100'000u},
    {      100u,  10'000u},
    {    1'000u,   1'000u},
    {   10'000u,     100u},
    {  100'000u,      10u},
    {1'000'000u,       3u}
};

static const std::vector<std::pair<size_t, size_t>> & detailedElementRoundCounts{false ? detailedElementRoundCountsDebug : detailedElementRoundCountsRelease};

static const size_t detailedChartRows{std::max(detailedElementRoundCountsRelease.size(), detailedElementRoundCountsDebug.size())};

static const std::vector<std::pair<size_t, size_t>> typicalElementRoundCounts {
#if 1
    {         32u,   500'000u},
    {        200u,   100'000u},
    {      3'000u,    10'000u},
    {     40'000u,     1'000u},
#if 1
    {    500'000u,       60u},
    {  3'000'000u,        12u},
    { 10'000'000u,        4u},
    { 50'000'000u,        2u},
#endif
#else

#if 1
    {4,   1000000},
    {10,  1000000},
    {32,   600000},
    {110,  200000},
    {240,  120000},
    {600,  70000},
#endif
    {1024, 40000 },
    {1500, 30000 },
    {2048, 20000 },
    {3000, 15000 },
    {6000, 7000 },
    {8192,  5000 },
    {12000, 3000 },
    {16384, 2000 },
    {25000, 1000 },
    {32768, 600 },
    {45000, 500 },
    {60000, 400 },
    {100000,200 },
    {150000, 100},
    {200000, 80 },
    {300000, 50 },
    {400000, 40 },
    {600000,  30 },
    {800000,  20 },
    {1200000, 12 },
    {2200000, 10 },
#if 1
    {3600000, 8},
    {6000000, 5},
    {10000000,3},
    {50000000,2},
#endif

#endif
};

enum class Stat : size_t
{
    objectSize,
    iteratorSize,
    memoryOverhead,
    construct,
    insert,
    insertReserved,
    insertPresent,
    accessPresent,
    accessAbsent,
    accessEmpty,
    iterateFull,
    iterateHalf,
    iterateEmpty,
    erase,
    eraseAbsent,
    refill,
    clear,
    loneBegin,
    loneEnd,
    destruction,
    _n
};

static const std::array<std::string, size_t(Stat::_n)> statNames{
    "ObjectSize",
    "IteratorSize",
    "MemoryOverhead",
    "Construct",
    "Insert",
    "InsertReserved",
    "InsertPresent",
    "AccessPresent",
    "AccessAbsent",
    "AccessEmpty",
    "IterateFull",
    "IterateHalf",
    "iterateEmpty",
    "Erase",
    "EraseAbsent",
    "Refill",
    "Clear",
    "LoneBegin",
    "LoneEnd",
    "Destruction"
};

#if QC_HASH
template <size_t size> struct Trivial;

template <size_t size> requires (size <= 8)
    struct Trivial<size>
{
    typename qc::sized<size>::utype val;
};

template <size_t size> requires (size > 8)
    struct Trivial<size>
{
    std::array<uint64_t, size / 8> val;
};

static_assert(std::is_trivial_v<Trivial<1>>);
static_assert(std::is_trivial_v<Trivial<8>>);
static_assert(std::is_trivial_v<Trivial<64>>);

template <size_t size>
class Complex : public Trivial<size>
{
public:

    constexpr Complex() : Trivial<size>{} {}

    constexpr Complex(const Complex & other) = default;

    constexpr Complex(Complex && other) noexcept :
        Trivial<size>{std::exchange(other.val, {})}
    {}

    Complex & operator=(const Complex &) = delete;

    Complex && operator=(Complex && other) noexcept {
        //Trivial::val = std::exchange(other.val, {});
    }

    constexpr ~Complex() noexcept {}
};

static_assert(!std::is_trivial_v<Complex<1>>);
static_assert(!std::is_trivial_v<Complex<8>>);
static_assert(!std::is_trivial_v<Complex<64>>);

template <size_t size> requires (size <= sizeof(size_t))
struct qc::hash::IdentityHash<Trivial<size>>
{
    constexpr size_t operator()(const Trivial<size> k) const noexcept
    {
        return k.val;
    }
};

template <size_t size> requires (size <= sizeof(size_t))
struct qc::hash::IdentityHash<Complex<size>>
{
    constexpr size_t operator()(const Complex<size> & k) const noexcept
    {
        return k.val;
    }
};
#endif

class Stats
{
public:

    double & get(const size_t containerI, const size_t elementCount, const Stat stat)
    {
        _presentContainerIndices.insert(containerI);
        _presentElementCounts.insert(elementCount);
        _presentStats.insert(stat);
        return _table[containerI][elementCount][stat];
    }

    double & at(const size_t containerI, const size_t elementCount, const Stat stat)
    {
        return const_cast<double &>(const_cast<const Stats *>(this)->at(containerI, elementCount, stat));
    }

    const double & at(const size_t containerI, const size_t elementCount, const Stat stat) const
    {
        return _table.at(containerI).at(elementCount).at(stat);
    }

    const std::set<size_t> presentContainerIndices() const { return _presentContainerIndices; }

    const std::set<size_t> presentElementCounts() const { return _presentElementCounts; }

    const std::set<Stat> presentStats() const { return _presentStats; }

    void setContainerNames(const std::vector<std::string> & containerNames)
    {
        if (containerNames.size() != _presentContainerIndices.size()) //throw std::exception{};
        _containerNames = containerNames;
    }

    template <typename... ContainerInfos>
    void setContainerNames()
    {
        _containerNames.clear();
        (_containerNames.push_back(ContainerInfos::name), ...);
    }

    const std::string & containerName(const size_t containerI) const
    {
        return _containerNames.at(containerI);
    }

//    private:

    std::map<size_t, std::map<size_t, std::map<Stat, double>>> _table{};
    std::set<size_t> _presentContainerIndices{};
    std::set<size_t> _presentElementCounts{};
    std::set<Stat> _presentStats{};
    std::vector<std::string> _containerNames{};
};

static void printTime(const int64_t nanoseconds, const size_t width)
{
    if (nanoseconds < 10000) {
        std::cout << std::setw(width - 3) << nanoseconds << " ns";
        return;
    }

    const int64_t microseconds{(nanoseconds + 500) / 1000};
    if (microseconds < 10000) {
        std::cout << std::setw(width - 3) << microseconds << " us";
        return;
    }

    const int64_t milliseconds{(microseconds + 500) / 1000};
    if (milliseconds < 10000) {
        std::cout << std::setw(width - 3) << milliseconds << " ms";
        return;
    }

    const int64_t seconds{(milliseconds + 500) / 1000};
    std::cout << std::setw(width - 3) << seconds << " s ";
}

static void printFactor(const int64_t t1, const int64_t t2, const size_t width)
{
    const double absFactor{t1 >= t2 ? double(t1) / double(t2) : double(t2) / double(t1)};
    int percent{int(std::round(absFactor * 100.0)) - 100};
    if (t1 < t2) {
        percent = -percent;
    }
    std::cout << std::setw(width - 2) << percent << " %";
}

#pragma warning(suppress: 4505)
static void reportComparison(const Stats & results, const size_t container1I, const size_t container2I, const size_t elementCount)
{
    const std::string c1Header = std::to_string(elementCount) + " Elements"; //{std::format("{:d} Elements", elementCount)};
    const std::string c4Header{"% Faster"};

    const std::string name1{results.containerName(container1I)};
    const std::string name2{results.containerName(container2I)};

    size_t c1Width{c1Header.size()};
    for (const Stat stat : results.presentStats()) {
        std::max(c1Width, statNames[size_t(stat)].size());
    }
    const size_t c2Width{std::max(name1.size(), size_t(7u))};
    const size_t c3Width{std::max(name2.size(), size_t(7u))};
    const size_t c4Width{std::max(c4Header.size(), size_t(8u))};

//    std::cout << std::format(" {:^{}} | {:^{}} | {:^{}} | {:^{}} ", c1Header, c1Width, name1, c2Width, name2, c3Width, c4Header, c4Width) << std::endl;
    printf("%20s:%zd | %s:%zd | %s:%zd | %s:%zd\n",
            c1Header.data(), c1Width,
            name1.data(), c2Width,
            name2.data(), c3Width,
            c4Header.data(), c4Width);
    std::cout << std::setfill('-') << std::setw(c1Width + 5u) << "+" << std::setw(c2Width + 5u) << "+" << std::setw(c3Width + 5u) << "+" << std::setw(c4Width + 2u) << "" << std::setfill(' ') << std::endl;

    for (const Stat stat : results.presentStats()) {
        const int64_t t1{int64_t(std::round(results.at(container1I, elementCount, stat)))};
        const int64_t t2{int64_t(std::round(results.at(container2I, elementCount, stat)))};

//        std::cout << std::format(" {:^{}} | ", statNames[size_t(stat)], c1Width);
    printf("%20s:%zd | ", statNames[size_t(stat)].data(), c1Width);
        printTime(t1, c2Width);
        std::cout << " | ";
        printTime(t2, c3Width);
        std::cout << " | ";
        printFactor(t1, t2, c4Width);
        std::cout << std::endl;
    }
}

#pragma warning(suppress: 4505)
static void printOpsChartable(const Stats & results, std::ostream & ofs)
{
    for (const Stat stat : results.presentStats()) {
        ofs << statNames[size_t(stat)] << ','; for (const size_t containerI : results.presentContainerIndices()) ofs << results.containerName(containerI) << ','; ofs << std::endl;

        size_t lineCount{0u};
        for (const size_t elementCount : results.presentElementCounts()) {
            ofs << elementCount << ',';
            for (const size_t containerI : results.presentContainerIndices()) {
                ofs << results.at(containerI, elementCount, stat) << ',';
            }
            ofs << std::endl;
            ++lineCount;
        }
        for (; lineCount < detailedChartRows; ++lineCount) {
            ofs << std::endl;
        }
    }
}

#pragma warning(suppress: 4505)
static void printTypicalChartable(const Stats & results, std::ostream & ofs)
{
    for (const size_t elementCount : results.presentElementCounts()) {
        auto str = std::to_string(elementCount);
        std::string padding = std::string(9 - str.size(), ' ');
        ofs << "|" <<elementCount << padding << "hashmap" << "|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|" << std::endl << std::setprecision(3);
        ofs << "|----------------|------|-----|-----|-----|-----|----------|" << std::endl;

        //ofs.fill('0');
        for (const size_t containerI : results.presentContainerIndices()) {
            ofs << "|" << results.containerName(containerI) << std::showpoint;
            ofs << "|  " << results.at(containerI, elementCount, Stat::insertReserved);
            ofs << "| " << results.at(containerI, elementCount, Stat::accessPresent);
            ofs << "| " << results.at(containerI, elementCount, Stat::accessEmpty);
            ofs << "| " << results.at(containerI, elementCount, Stat::erase);
            ofs << "| " << std::to_string(results.at(containerI, elementCount, Stat::iterateFull)).substr(0,4);
            ofs << "| " << results.at(containerI, elementCount, Stat::iterateHalf);
            ofs << "     |"<< std::endl;
        }
        ofs << std::endl << std::endl;
    }
}

template <typename Container, typename K>
static void time(const size_t containerI, const std::span<const K> presentKeys, const std::span<const K> absentKeys, Stats & results)
{
    static_assert(std::is_same_v<K, typename Container::key_type>);

    static constexpr bool isSet{!IsMap<Container>};

//    static volatile size_t v{};

    const std::span<const K> firstHalfPresentKeys{&presentKeys[0], presentKeys.size() / 2};
    const std::span<const K> secondHalfPresentKeys{&presentKeys[presentKeys.size() / 2], presentKeys.size() / 2};
    const double invElementCount{1.0 / double(presentKeys.size())};
    const double invHalfElementCount{invElementCount * 2.0};

    alignas(Container) std::byte backingMemory[sizeof(Container)];
    Container * containerPtr;

    std::array<double, size_t(Stat::_n)> stats{};

    // Construct
    {
        const int64_t t0{now()};

        containerPtr = new (backingMemory) Container{};

        stats[size_t(Stat::construct)] += double(now() - t0);
    }

    Container & container{*containerPtr};

    // Insert to full capacity
    {
        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            if constexpr (isSet) {
                container.emplace(key);
            }
            else {
                container.emplace(key, typename Container::mapped_type{});
            }
        }

        stats[size_t(Stat::insert)] += double(now() - t0) * invElementCount;
    }

    // Full capacity insert present elements
    {
        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            if constexpr (isSet) {
                container.emplace(key);
            }
            else {
                container.emplace(key, typename Container::mapped_type{});
            }
        }

        stats[size_t(Stat::insertPresent)] += double(now() - t0) * invElementCount;
    }

    auto v = 0;
    // Full capacity access present elements
    {
        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            v = v + container.count(reinterpret_cast<const typename Container::key_type &>(key));
        }

        stats[size_t(Stat::accessPresent)] += double(now() - t0) * invElementCount;
    }

    // Full capacity access absent elements
    {
        const int64_t t0{now()};

        for (const K & key : absentKeys) {
            v = v + container.count(key);
        }

        stats[size_t(Stat::accessAbsent)] += double(now() - t0) * invElementCount;
    }

    // Full capacity iteration
    {
        const int64_t t0{now()};

        for (const auto & element : container) {
            // Important to actually use the value as to load the memory
            if constexpr (isSet) {
                v = v + reinterpret_cast<const size_t &>(element);
            }
            else {
                v = v + reinterpret_cast<const size_t &>(element.first);
            }
        }

        stats[size_t(Stat::iterateFull)] += double(now() - t0) * invElementCount;
    }

    // Full capacity erase absent elements
    {
        const int64_t t0{now()};

        for (const K & key : absentKeys) {
            container.erase(key);
        }

        stats[size_t(Stat::eraseAbsent)] += double(now() - t0) * invElementCount;
    }

    // Half erasure
    {
        const int64_t t0{now()};

        for (const K & key : secondHalfPresentKeys) {
            container.erase(key);
        }

        stats[size_t(Stat::erase)] += double(now() - t0) * invHalfElementCount;
    }

    // Half capacity iteration
    {
        const int64_t t0{now()};

        for (const auto & element : container) {
            // Important to actually use the value as to load the memory
            if constexpr (isSet) {
                v = v + reinterpret_cast<const size_t &>(element);
            }
            else {
                v = v + reinterpret_cast<const size_t &>(element.first);
            }
        }

        stats[size_t(Stat::iterateHalf)] += double(now() - t0) * invHalfElementCount;
    }

    // Erase remaining elements
    {
        const int64_t t0{now()};

        for (const K & key : firstHalfPresentKeys) {
            container.erase(key);
        }

        stats[size_t(Stat::erase)] += double(now() - t0) * invHalfElementCount;
    }

    // Empty access
    {
        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            v = v + container.count(key);
        }

        stats[size_t(Stat::accessEmpty)] += double(now() - t0) * invElementCount;
    }

    // Empty iteration
    {
        const int64_t t0{now()};

        for (const auto & element : container) {
            // Important to actually use the value as to load the memory
            if constexpr (isSet) {
                v = v + reinterpret_cast<const size_t &>(element);
            }
            else {
                v = v + reinterpret_cast<const size_t &>(element.first);
            }
        }

        stats[size_t(Stat::iterateEmpty)] += double(now() - t0);
    }

    // Insert single element
    if constexpr (isSet) {
        container.emplace(presentKeys.front());
    }
    else {
        container.emplace(presentKeys.front(), typename Container::mapped_type{});
    }

    // Single element begin
    {
        const int64_t t0{now()};

        volatile const auto it{container.cbegin()};

        stats[size_t(Stat::loneBegin)] += double(now() - t0);
    }

    // Single element end
    {
        const int64_t t0{now()};

        volatile const auto it{container.cend()};

        stats[size_t(Stat::loneEnd)] += double(now() - t0);
    }

    // Erase single element
    container.erase(presentKeys.front());

    // Reinsertion
    {
        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            if constexpr (isSet) {
                container.emplace(key);
            }
            else {
                container.emplace(key, typename Container::mapped_type{});
            }
        }

        stats[size_t(Stat::refill)] += double(now() - t0) * invElementCount;
    }

    // Clear
    {
        const int64_t t0{now()};

        container.clear();

        stats[size_t(Stat::clear)] += double(now() - t0) * invElementCount;
    }

    // Reserved insertion
    {
        container.reserve(presentKeys.size());

        const int64_t t0{now()};

        for (const K & key : presentKeys) {
            if constexpr (isSet) {
                container.emplace(key);
            }
            else {
                container.emplace(key, typename Container::mapped_type{});
            }
        }

        stats[size_t(Stat::insertReserved)] += double(now() - t0) * invElementCount;
    }

    // Destruct
    {
        const int64_t t0{now()};

        container.~Container();

        stats[size_t(Stat::destruction)] += double(now() - t0) * invElementCount;
    }

    for (size_t i{}; i < stats.size(); ++i) {
        results.get(containerI, presentKeys.size(), Stat(i)) += stats[i];
    }
}

template <typename Container, typename K>
static void timeTypical(const size_t containerI, Container& container, const std::span<const K> keys, Stats & results)
{
    static_assert(std::is_same_v<K, typename Container::key_type>);

    static constexpr bool isSet{!IsMap<Container>};
    static size_t v{0};

    const double invElementCount{1.0 / double(keys.size())};

    const int64_t t0{now()};
    // Insert
    for (const K key : keys) {
        if constexpr (isSet) {
            container.emplace(key);
        }
        else {
            container.emplace(key, typename Container::mapped_type{});
        }
    }

    int64_t t1{now()};
    results.get(containerI, keys.size(), Stat::insertReserved)+= double(t1 - t0) * invElementCount;

    //container.reserve(2);
    t1 = now();
    // Access
    for (const K key : keys) {
        v = v + container.count(key);
    }

    const int64_t t2{now()};
    // AccessEmpty
    v = 1;
    size_t hit = 0;
    for (const K key : keys) {
        hit += container.count(key + v);
    }
    v = hit;

    const int64_t t3{now()};
    // Iterate
    for (const auto element : container) {
        // Important to actually use the value as to load the memory
        if constexpr (isSet) {
            v = v + reinterpret_cast<const size_t &>(element);
        }
        else {
            v = v + 1;//reinterpret_cast<const size_t &>(element.first);
        }
    }

    auto lf = container.load_factor();
    const int64_t t4{now()};
    // Erase
    for (const K key : keys) {
        v += container.erase(key);
    }

    assert(v != 0);

    const int64_t t5{now()};
    results.get(containerI, keys.size(), Stat::accessPresent) += double(t2 - t1) * invElementCount;
    results.get(containerI, keys.size(), Stat::accessEmpty)   += double(t3 - t2) * invElementCount;
    results.get(containerI, keys.size(), Stat::iterateFull)   += double(t4 - t3) * invElementCount;
    results.get(containerI, keys.size(), Stat::erase)         += double(t5 - t4) * invElementCount;
    results.get(containerI, keys.size(), Stat::iterateHalf)   += lf * 100;
}

template <typename CommonKey, typename ContainerInfo, typename... ContainerInfos>
static void timeContainers(const size_t containerI, const std::vector<CommonKey> & presentKeys, const std::vector<CommonKey> & absentKeys, Stats & results)
{
    using Container = typename ContainerInfo::Container;

    if constexpr (!std::is_same_v<Container, void>) {
        using K = typename Container::key_type;
        static_assert(sizeof(CommonKey) == sizeof(K) && alignof(CommonKey) == alignof(K));

        const std::span<const K> presentKeys_{reinterpret_cast<const K *>(presentKeys.data()), presentKeys.size()};
        const std::span<const K> absentKeys_{reinterpret_cast<const K *>(absentKeys.data()), absentKeys.size()};
        time<Container>(containerI, presentKeys_, absentKeys_, results);
    }

    if constexpr (sizeof...(ContainerInfos) != 0u) {
        timeContainers<CommonKey, ContainerInfos...>(containerI + 1u, presentKeys, absentKeys, results);
    }
}

template <typename CommonKey, typename ContainerInfo, typename... ContainerInfos>
static void timeContainersTypical(const size_t containerI, const std::vector<CommonKey> & keys, Stats & results)
{
    using Container = typename ContainerInfo::Container;

    if constexpr (!std::is_same_v<Container, void>) {
        using K = typename Container::key_type;
        static_assert(sizeof(CommonKey) == sizeof(K) && alignof(CommonKey) == alignof(K));

        const std::span<const K> keys_{reinterpret_cast<const K *>(keys.data()), keys.size()};
        Container container;
        container.max_load_factor(0.875);
        container.reserve(keys_.size() / 2);
        timeTypical<Container>(containerI, container, keys_, results);
    }

    if constexpr (sizeof...(ContainerInfos) != 0u) {
        timeContainersTypical<CommonKey, ContainerInfos...>(containerI + 1u, keys, results);
    }
}

template <typename CommonKey, typename ContainerInfo, typename... ContainerInfos>
static void compareMemory(const size_t containerI, const std::vector<CommonKey> & keys, Stats & results)
{
    using Container = typename ContainerInfo::Container;
    using AllocatorContainer = typename ContainerInfo::AllocatorContainer;
    if constexpr (!std::is_same_v<AllocatorContainer, void>) static_assert(std::is_same_v<typename Container::key_type, typename AllocatorContainer::key_type>);
    using K = typename Container::key_type;
    static_assert(sizeof(CommonKey) == sizeof(K) && alignof(CommonKey) == alignof(K));

    const std::span<const K> keys_{reinterpret_cast<const K *>(keys.data()), keys.size()};

    if constexpr (!std::is_same_v<Container, void>) {
        results.get(containerI, keys.size(), Stat::objectSize) = sizeof(Container);
        results.get(containerI, keys.size(), Stat::iteratorSize) = sizeof(typename Container::iterator);
    }

    if constexpr (!std::is_same_v<AllocatorContainer, void>) {
        AllocatorContainer container{};
        container.reserve(keys_.size());
        for (const K & key : keys_) {
            if constexpr (IsMap<Container>) {
                container.emplace(key, typename Container::mapped_type{});
            }
            else {
                container.emplace(key);
            }
        }
        results.get(containerI, keys.size(), Stat::memoryOverhead) = double(container.get_allocator().stats().current - keys.size() * sizeof(K)) / double(keys.size());
    }

    if constexpr (sizeof...(ContainerInfos) != 0u) {
        compareMemory<CommonKey, ContainerInfos...>(containerI + 1u, keys, results);
    }
}

template <typename CommonKey, typename... ContainerInfos>
static void compareDetailedSized(const size_t elementCount, const size_t roundCount, Stats & results)
{
    auto nowms = getus();
    WyRand wyrandom(nowms);
    const double invRoundCount{1.0 / double(roundCount)};

    std::vector<CommonKey> presentKeys(elementCount);
    std::vector<CommonKey> absentKeys(elementCount);
    for (CommonKey & key : presentKeys) key = wyrandom();

    for (size_t round{0u}; round < roundCount; ++round) {
        std::swap(presentKeys, absentKeys);
        for (CommonKey & key : presentKeys) key = wyrandom();

        timeContainers<CommonKey, ContainerInfos...>(0u, presentKeys, absentKeys, results);
    }

    for (const size_t containerI : results.presentContainerIndices()) {
        for (const Stat stat : results.presentStats()) {
            results.at(containerI, elementCount, stat) *= invRoundCount;
        }
    }

    compareMemory<CommonKey, ContainerInfos...>(0u, presentKeys, results);
}

template <typename CommonKey, typename... ContainerInfos>
static void compareDetailed(Stats & results)
{
    for (const auto [elementCount, roundCount] : detailedElementRoundCounts) {
        std::cout << "Comparing " << elementCount << " elements " << roundCount << " rounds of ...";
        auto nowms = getus();
        compareDetailedSized<CommonKey, ContainerInfos...>(elementCount, roundCount, results);
        std::cout << " done use " << ((getus() - nowms) / 1e9) << " sec" << std::endl;
    }

    results.setContainerNames<ContainerInfos...>();
}

template <typename CommonKey, typename... ContainerInfos>
static void compareTypicalSized(const size_t elementCount, const size_t roundCount, Stats & results)
{
    std::vector<CommonKey> keys(elementCount);

    std::cout << "Comparing " << elementCount << " elements " << roundCount << " rounds of ...";
    auto nowms = getus();
    WyRand wyrandom(nowms);
    for (size_t round{0u}; round < roundCount; ++round) {
        for (CommonKey & key : keys) key = wyrandom(); //random.next<CommonKey>();
        timeContainersTypical<CommonKey, ContainerInfos...>(0u, keys, results);
    }
    std::cout << " done use " << ((getus() - nowms) / 1e6) << " sec" << std::endl;

    const double invRoundCount{1.0 / double(roundCount)};
    for (const size_t containerI : results.presentContainerIndices()) {
        for (const Stat stat : results.presentStats()) {
            results.at(containerI, elementCount, stat) *= invRoundCount;
        }
    }
}

template <typename CommonKey, typename... ContainerInfos>
static void compareTypical(Stats & results)
{
    for (const auto&[elementCount, roundCount] : typicalElementRoundCounts) {
        compareTypicalSized<CommonKey, ContainerInfos...>(elementCount, roundCount, results);
    }

    results.setContainerNames<ContainerInfos...>();
}

enum class CompareMode { oneVsOne, detailed, typical };

template <CompareMode mode, typename CommonKey, typename... ContainerInfos>
static void compare()
{
    static const std::filesystem::path outFilePath{"qbench-out.txt"};

    // 1-vs-1
    if constexpr (mode == CompareMode::oneVsOne) {
        static_assert(sizeof...(ContainerInfos) == 2);
        Stats results{};
        compareTypical<CommonKey, ContainerInfos...>(results);
        std::cout << std::endl;
        for (const auto& [elementCount, roundCount] : typicalElementRoundCounts) {
            reportComparison(results, 1, 0, elementCount);
            std::cout << std::endl;
        }
    }
    // Detailed
    else if constexpr (mode == CompareMode::detailed) {
        Stats results{};
        compareDetailed<CommonKey, ContainerInfos...>(results);
        std::ofstream ofs{outFilePath};
        printOpsChartable(results, ofs);
        std::cout << "Wrote results to " << outFilePath << std::endl;
    }
    // Typical
    else if constexpr (mode == CompareMode::typical) {
        Stats results{};
        compareTypical<CommonKey, ContainerInfos...>(results);
        std::ofstream ofs{outFilePath};
        printTypicalChartable(results, ofs);
        std::cout << "Wrote results to " << outFilePath << std::endl;
        printTypicalChartable(results, std::cout);
    }
}

#if QC_HASH
template <typename K, typename V, bool sizeMode = false, bool doTrivialComplex = false>
struct QcHashMapInfo
{
    using Container = qc::hash::RawMap<K, V>;
    //using AllocatorContainer = qc::hash::RawMap<K, V, QintHasher, qc::memory::RecordAllocator<std::pair<K, V>>>;
    using AllocatorContainer = void;

    static constexpr bool isKeyTrivial{std::is_same_v<K, Trivial<sizeof(K)>>};
    static constexpr bool isValTrivial{std::is_same_v<V, Trivial<sizeof(V)>>};
//    static inline const std::string name{sizeMode ? std::format("{}{} : {}{}", (doTrivialComplex ? isKeyTrivial ? "Trivial " : "Complex " : ""), sizeof(K), (doTrivialComplex ? isValTrivial ? "Trivial " : "Complex " : ""), sizeof(V)) : "qc::hash::RawMap"};
    static inline const std::string name{"qc__hash::RawMap"};
};
#endif

template <typename K>
struct StdSetInfo
{
    using Container = std::unordered_set<K>;
    using AllocatorContainer = std::unordered_set<K, typename std::unordered_set<K>::hasher, typename std::unordered_set<K>::key_equal>;

    static inline const std::string name{"std::unordered_map"};
};

template <typename K, typename V>
struct StdMapInfo
{
    using Container = std::unordered_map<K, V, QintHasher>;
    using AllocatorContainer = std::unordered_map<K, V, typename std::unordered_map<K, V>::hasher, typename std::unordered_map<K, V>::key_equal>;

    static inline const std::string name{"std::unorder_map"};
};

#if ABSL_HMAP
template <typename K>
struct AbslSetInfo
{
    //using Container = std::conditional_t<sizeof(size_t) == 8, absl::flat_hash_set<K>, void>;
    //using AllocatorContainer = std::conditional_t<sizeof(size_t) == 8, absl::flat_hash_set<K, typename absl::flat_hash_set<K>::hasher, typename absl::flat_hash_set<K>::key_equal, qc::memory::RecordAllocator<K>>, void>;
    using AllocatorContainer = void;

    static inline const std::string name{"absl::flat_hash_set"};
};

template <typename K, typename V>
struct AbslMapInfo
{
    using Container = absl::flat_hash_map<K, V, QintHasher>;
    //using AllocatorContainer = std::conditional_t<sizeof(size_t) == 8, std::unordered_map<K, V, typename absl::flat_hash_map<K, V>::hasher, typename absl::flat_hash_map<K, V>::key_equal, qc::memory::RecordAllocator<std::pair<K, V>>>, void>;
    using AllocatorContainer = void;

    static inline const std::string name{"absl::f_hash_map"};
};
#endif

template <typename K, typename V>
struct PhMapInfo
{
    using Container = phmap::flat_hash_map<K, V, QintHasher>;
    //using AllocatorContainer = std::conditional_t<sizeof(size_t) == 8, std::unordered_map<K, V, typename absl::flat_hash_map<K, V>::hasher, typename absl::flat_hash_map<K, V>::key_equal, qc::memory::RecordAllocator<std::pair<K, V>>>, void>;
    using AllocatorContainer = void;

    static inline const std::string name{"phmap::fhash_map"};
};


template <typename K>
struct RobinHoodSetInfo
{
    using Container = robin_hood::unordered_set<K>;
    using AllocatorContainer = void;

    static inline const std::string name{"martinus::fhset"};
};

template <typename K, typename V>
struct RobinHoodMapInfo
{
    using Container = robin_hood::unordered_flat_map<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"martinus::fhmap "};
};

template <typename K, typename V>
struct RobinDenseMapInfo
{
    using Container = ankerl::unordered_dense::map<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"martinus::dense "};
};

#if CXX20
template <typename K, typename V>
struct FphDyamicMapInfo
{
    using Container = fph::DynamicFphMap<K, V, fph::MixSeedHash<K>>;
    using AllocatorContainer = void;
    static inline const std::string name{"fph::DynamicFph "};
};

#if HAVE_BOOST
template <typename K, typename V>
struct BoostFlapMapInfo
{
    using Container = boost::unordered_flat_map<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"boost::fhash_map"};
};
#endif
#endif

#if X86_64
template <typename K, typename V>
struct HrdmHashMap
{
    using Container = hrd_m::hash_map<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"hrd_m::fhash_map"};
};
#endif

template <typename K>
struct SkaSetInfo
{
    using Container = ska::flat_hash_set<K>;
    using AllocatorContainer = ska::flat_hash_set<K, typename ska::flat_hash_set<K>::hasher, typename ska::flat_hash_set<K>::key_equal>;

    static inline const std::string name{"ska::flat_hash_set"};
};

template <typename K, typename V>
struct SkaMapInfo
{
    using Container = ska::flat_hash_map<K, V, QintHasher>;
    using AllocatorContainer = void;
    //using AllocatorContainer = ska::flat_hash_map<K, V, QintHasher, typename ska::flat_hash_map<K, V>::key_equal, qc::memory::RecordAllocator<std::pair<K, V>>>;

    static inline const std::string name{"ska:flat_hashmap"};
};

template <typename K>
struct TslRobinSetInfo
{
    using Container = tsl::robin_set<K>;
    //using AllocatorContainer = tsl::robin_set<K, typename tsl::robin_set<K>::hasher, typename tsl::robin_set<K>::key_equal, qc::memory::RecordAllocator<K>>;
    using AllocatorContainer = void;

    static inline const std::string name{"tsl::robin_set"};
};

template <typename K, typename V>
struct TslRobinMapInfo
{
    using Container = tsl::robin_map<K, V, QintHasher>;
    //using AllocatorContainer = tsl::robin_map<K, V, typename tsl::robin_map<K, V>::hasher, typename tsl::robin_map<K, V>::key_equal, qc::memory::RecordAllocator<std::pair<K, V>>>;
    using AllocatorContainer = void;

    static inline const std::string name{"tsl::robin_map  "};
};

template <typename K>
struct TslSparseSetInfo
{
    using Container = tsl::sparse_set<K>;
    using AllocatorContainer = void;

    static inline const std::string name{"tsl::sparse_hash_set"};
};

template <typename K, typename V>
struct TslSparseMapInfo
{
    using Container = tsl::sparse_map<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"tsl::sparse_hash_map"};
};

template <typename K, typename V>
struct EmHash5MapInfo
{
    using Container = emhash5::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"emhash5::HashMap"};
};

template <typename K, typename V>
struct EmHash6MapInfo
{
    using Container = emhash6::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"emhash6::HashMap"};
};

template <typename K, typename V>
struct EmHash7MapInfo
{
    using Container = emhash7::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emhash7::HashMap"};
};

template <typename K, typename V>
struct EmHash8MapInfo
{
    using Container = emhash8::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emhash8::HashMap"};
};


template <typename K, typename V>
struct EmiLib2MapInfo
{
    using Container = emilib2::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emilib2::HashMap"};
};

template <typename K, typename V>
struct EmiLib3MapInfo
{
    using Container = emilib3::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emilib3::HashMap"};
};

template <typename K, typename V>
struct EmiLib1MapInfo
{
    using Container = emilib::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emilib1::HashMap"};
};

#if 0
template <typename K, typename V>
struct EmLibMapInfo
{
    using Container = emlib2::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emilib::HashMap "};
};
#endif

template <typename K, typename V>
struct JgDenseMapInfo
{
    using Container = jg::dense_hash_map<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"jg::den_hash_map"};
};

template <typename K, typename V>
struct RigtorpMapInfo
{
    //using Container = rigtorp::HashMap<K, V, QintHasher>;
    using Container = rigtorp::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"rigtorp::HashMap"};
};

#if CK_HMAP
template <typename K, typename V>
struct CkHashMapInfo
{
    using Container = ck::HashMap<K, V, QintHasher>;
    using AllocatorContainer = void;

    static inline const std::string name{"ck::f_hash_map  "};
};
#endif

#if 0
template <typename K>
struct EmiLib2SetInfo
{
    using Container = emilib2::HashSet<K, typename qc::hash::RawSet<K>::hasher>;
    using AllocatorContainer = void;
    static inline const std::string name{"emilib2::HashSet"};
};
#endif


int main(const int argc, const char* argv[])
{
    printInfo(nullptr);
    // 1v1
    if (argc == 2) {
        using K = size_t;
//        compare<CompareMode::oneVsOne, K, QcHashSetInfo<K>, AbslSetInfo<K>>();
//        compare<CompareMode::oneVsOne, K, QcHashSetInfo<K>, EmiLib2SetInfo<K>>();
//        compare<CompareMode::oneVsOne, K, EmiLib3MapInfo<K,int>, EmiLib2MapInfo<K, int>>();
//        compare<CompareMode::oneVsOne, K, QcHashMapInfo<K,int>, EmHash7MapInfo<K, int>>();
#if 1
//        compare<CompareMode::oneVsOne, K, AbslMapInfo<K,int>, EmiLib3MapInfo<K, int>>();
//        compare<CompareMode::oneVsOne, K, RigtorpMapInfo<K, uint64_t>, CkHashMapInfo<K, uint64_t>>();
#endif
//        compare<CompareMode::oneVsOne, K, EmHash6MapInfo<K,int>, EmHash5MapInfo<K, int>>();

    }
    // Set comparison
    else if constexpr (false) {
        using K = size_t;
        compare<CompareMode::typical, K,
            //QcHashSetInfo<K>,
            StdSetInfo<K>,
            //AbslSetInfo<K>,
            RobinHoodSetInfo<K>,
            SkaSetInfo<K>,
            TslRobinSetInfo<K>,
            TslSparseSetInfo<K>
        >();
    }
    // Map comparison
    if constexpr (true) {
#if QKey == 0
        using K = size_t;
#else
        using K = uint32_t;
#endif

#if TVal == 0
        using V = size_t;
#elif TVal == 1
        using V = uint32_t;
#else
        using V = std::string;
#endif

        compare<CompareMode::typical, K,

            EmiLib1MapInfo<K, V>,
            EmiLib3MapInfo<K, V>,
            EmiLib2MapInfo<K, V>,
//            EmLibMapInfo<K, V>,

#if HAVE_BOOST
            BoostFlapMapInfo<K, V>,
#endif
#if ABSL_HMAP
            AbslMapInfo<K, V>,
#endif

#if ET
            PhMapInfo<K, V>,
            RobinHoodMapInfo<K, V>,

            SkaMapInfo<K, V>,
            TslRobinMapInfo<K, V>,
#if ET > 1
            StdMapInfo<K, V>,
            TslSparseMapInfo<K, V>,
#endif
#endif

            RobinDenseMapInfo<K, V>,
            EmHash8MapInfo<K, V>,

#if CXX23
            //FphDyamicMapInfo<K,V>,
            JgDenseMapInfo<K, V>,
#if X86_64
            HrdmHashMap<K, V>,
            RigtorpMapInfo<K, V>,
#endif
#if QC_HASH
            QcHashMapInfo<K, V>,
#endif
#endif

#if CK_HMAP
            CkHashMapInfo<K, V>,
#endif

            EmHash7MapInfo<K, V>,
            EmHash6MapInfo<K, V>,
            EmHash5MapInfo<K, V>
        >();
    }

#if QC_HASH_
    // Architecture comparison
    else if constexpr (false) {
        using K = u32;
        compare<CompareMode::typical, K, QcHashSetInfo<K>>();
    }
    // Set vs map
    if constexpr (false) {
        compare<CompareMode::detailed, size_t,
            QcHashSetInfo<size_t, true>,
            QcHashMapInfo<size_t, Trivial<8>, true>,
            QcHashMapInfo<size_t, Trivial<16>, true>,
            QcHashMapInfo<size_t, Trivial<32>, true>,
            QcHashMapInfo<size_t, Trivial<64>, true>,
            QcHashMapInfo<size_t, Trivial<128>, true>,
            QcHashMapInfo<size_t, Trivial<256>, true>>();
    }
    // Trivial vs complex
    else if constexpr (false) {
        using K = size_t;
        compare<CompareMode::detailed, size_t,
//            QcHashSetInfo<Trivial<sizeof(K)>, true, true>,
//            QcHashSetInfo<Complex<sizeof(K)>, true, true>,
            QcHashMapInfo<Trivial<sizeof(K)>, Trivial<sizeof(K)>, true, true>,
//            QcHashMapInfo<Complex<sizeof(K)>, Trivial<sizeof(K)>, true, true>,
//            QcHashMapInfo<Trivial<sizeof(K)>, Complex<sizeof(K)>, true, true>,
            QcHashMapInfo<Complex<sizeof(K)>, Complex<sizeof(K)>, true, true> >();
    }
#endif

    return 0;
}
