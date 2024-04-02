#include <iomanip>
#include "util.h"
#include <typeinfo>

#ifndef _WIN32
#include <cxxabi.h>
#endif
#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#include "ska/flat_hash_map.hpp"
#include "ska/bytell_hash_map.hpp"
#include "martin/robin_hood.h"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_set2.hpp"
#include "hash_set4.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"
#include "emilib/emiset.hpp"

// https://github.com/Tessil/robin-map
#include "tsl/robin_set.h"
#include "tsl/robin_map.h"
#include "phmap/phmap.h"
#include "phmap/btree.h"

#include "has_member.hpp"
define_has_member(reserve);

using namespace std;
namespace cr = chrono;

std::string& inplace_replace_all(std::string& s, const std::string& from, const std::string& to)
{
    if (!from.empty())
        for (size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size())
            s.replace(pos, from.size(), to);
    return s;
}

using Sample = long;                ///< Sample.
using Samples = std::vector<Sample>; ///< Samples.
using Clock = cr::high_resolution_clock;
using Dur = decltype(Clock::now() - Clock::now()); ///< Duration.
using Durs = std::vector<Dur>;                     ///< Durations.

template<class T>
void showHeader()
{
    int status;
#ifndef _WIN32
    std::string name = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
#else
    const char* cname = typeid(T).name() + 0;
    if (strstr(cname, "class") != 0)
        cname += 6;
    else
        cname += 2;

    while ((cname[0] >= '0' && cname[0] <= 9) || cname[0] == ' ')
        cname ++;
    string name = cname;
#endif

    auto c = name.find('<');
    if (c != string::npos)
        name = name.substr(0, c);
    name = inplace_replace_all(name, "unsigned long", "ulong");
    if (name.size() > 60)
        name.resize(60);
    cout << "--- " << name << ":" << endl;
}

void showResults(const string& tag, const Durs& durs, size_t elementCount, bool okFlag)
{
    const auto min_dur = *min_element(begin(durs), end(durs));
    const auto dur_ns = cr::duration_cast<cr::nanoseconds>(min_dur).count();
    cout << tag << ":"
         << fixed << right << setprecision(1) << setw(4) << setfill(' ')
         << (static_cast<double>(dur_ns)) / elementCount
         << (okFlag ? "" : " ERR")
         << ", ";
}

template<class Vector>
void benchmarkVector(const Samples& ulongArray, const size_t runCount)
{
    cout << "- ";
    Vector x;
    if constexpr (has_member(Vector, reserve))
    {
        x.reserve(ulongArray.size());
    }

    Durs durs(runCount);

    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        auto beg = Clock::now();
        for (size_t i = 0; i < ulongArray.size(); ++i)
            x.push_back(ulongArray[i]);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("push_back", durs, ulongArray.size(), true);

    showHeader<Vector>();
}

template<class Set, bool reserveFlag>
Set benchmarkSet_insert(const Samples& ulongArray,
                         const size_t runCount)
{
    Durs durs(runCount);

    Set x;

    if constexpr (reserveFlag)
        x.reserve(ulongArray.size());

    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            x.insert(e);
        durs[runIx] = Clock::now() - beg;
    }
    if constexpr (reserveFlag)
        showResults("insert (reserved)", durs, ulongArray.size(), true);
    else
        showResults("insert", durs, ulongArray.size(), true);

    return x;
}

template<class Set>
void benchmarkSet(const Samples& ulongArray,
                  const size_t runCount)
{
    cout << "- ";

    if constexpr (has_member(Set, reserve))
    {
        const Set __attribute__((unused)) x = benchmarkSet_insert<Set, true>(ulongArray, runCount);
    }

    Durs durs(runCount);

    Set x = benchmarkSet_insert<Set, false>(ulongArray, runCount);

    int allHit = 0;
    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            allHit += x.count(e);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("find", durs, ulongArray.size(), allHit == ulongArray.size() * runCount);

    int allErase = 0;
    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            allErase += x.erase(e);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("erase", durs, ulongArray.size(), allErase == ulongArray.size());

    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            x.insert(e);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("reinsert", durs, ulongArray.size(), true);

    x.clear();

    showHeader<Set>();
}

template<class Map>
void benchmarkMap(const Samples& ulongArray, const size_t runCount)
{
    cout << "- ";
    Map x;
    if constexpr (has_member(Map, reserve))
    {
        x.reserve(ulongArray.size());
    }

    Durs durs(runCount);

    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            x[e] = 0;
        durs[runIx] = Clock::now() - beg;
    }
    showResults("insert", durs, ulongArray.size(), true);

    int allHit = 0;
    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            allHit += x.count(e);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("find", durs, ulongArray.size(), allHit == ulongArray.size() * runCount);

    int allErase = 0;
    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            allErase += x.erase(e);
        durs[runIx] = Clock::now() - beg;
    }
    showResults("erase", durs, ulongArray.size(), allErase == ulongArray.size());

    for (size_t runIx = 0; runIx != runCount; ++runIx)
    {
        const auto beg = Clock::now();
        for (const auto e : ulongArray)
            x[e] = 1;
        durs[runIx] = Clock::now() - beg;
    }
    showResults("reinsert", durs, ulongArray.size(), true);

    x.clear();

    showHeader<Map>();
}

Samples getSource(size_t elementCount)
{
    Samples source(elementCount);
    for (size_t i = 0; i < elementCount; ++i)
    {
        source[i] = i;
    }
    shuffle(std::begin(source), std::end(source));
    return source;
}

void benchmarkAllUnorderedSets(const Samples& ulongArray,
                               const size_t runCount)
{
    benchmarkSet<tsl::robin_set<Sample>>(ulongArray, runCount);
    benchmarkSet<tsl::robin_set<Sample>>(ulongArray, runCount);
    benchmarkSet<tsl::robin_pg_set<Sample>>(ulongArray, runCount);
    benchmarkSet<ska::flat_hash_set<Sample>>(ulongArray, runCount);
    /* TODO: benchmarkSet<ska::bytell_hash_set<Sample>>(ulongArray, runCount); */
    benchmarkSet<robin_hood::unordered_flat_set<Sample>>(ulongArray, runCount);
    benchmarkSet<robin_hood::unordered_node_set<Sample>>(ulongArray, runCount);
    benchmarkSet<robin_hood::unordered_set<Sample>>(ulongArray, runCount);
    benchmarkSet<std::unordered_set<Sample>>(ulongArray, runCount);
    benchmarkSet<std::unordered_multiset<Sample>>(ulongArray, runCount);
    benchmarkSet<emhash2::HashSet<Sample>>(ulongArray, runCount);
    benchmarkSet<emhash9::HashSet<Sample>>(ulongArray, runCount);
    benchmarkSet<phmap::flat_hash_set<Sample>>(ulongArray, runCount);
#if QC_HASH
    benchmarkSet<fph::DynamicFphSet<Sample, fph::MixSeedHash<Sample>>>(ulongArray, runCount);
//    benchmarkSet<qc::hash::RawSet<Sample>>(ulongArray, runCount);
#endif
    benchmarkSet<emilib::HashSet<Sample>>(ulongArray, runCount);
}

int main(__attribute__((unused)) int argc,
         __attribute__((unused)) const char* argv[],
         __attribute__((unused)) const char* envp[])
{
    size_t elementCount = 2234567; ///< Number of elements.
    const size_t runCount = 3;          ///< Number of runs per benchmark.
    if (argc > 1)
        elementCount = atoi(argv[1]);

    const auto ulongArray = getSource(elementCount);

    cout << "# Vector:" << endl;
    benchmarkVector<std::vector<Sample>>(ulongArray, runCount);

    cout << "\n# Unordered Sets:" << endl;
    benchmarkAllUnorderedSets(ulongArray, runCount);
    cout << "===================================================" << endl;

    cout << "\n# Ordered Sets:" << endl;
    benchmarkSet<std::set<Sample>>(ulongArray, runCount);
    benchmarkSet<std::multiset<Sample>>(ulongArray, runCount);
    benchmarkSet<phmap::btree_set<Sample>>(ulongArray, runCount);

#ifdef HOOD_HASH
    using hash_t = robin_hood::hash<Sample>;
#elif ABSL_HASH
    using hash_t = absl::Hash<Sample>;
#elif FIB_HASH
    using hash_t = Int64Hasher<Sample>;
#else
    using hash_t = std::hash<Sample>;
#endif


    cout << "\n# Unordered Maps:" << endl;
    benchmarkMap<phmap::flat_hash_map<Sample, Sample, hash_t>>(ulongArray, runCount);
#if ABSL_HMAP
    benchmarkMap<absl::flat_hash_map<Sample, Sample, hash_t>>(ulongArray, runCount);
#endif

#if QC_HASH
    benchmarkMap<fph::DynamicFphMap<Sample, Sample, fph::MixSeedHash<Sample>>>(ulongArray, runCount);
    benchmarkMap<qc::hash::RawMap<Sample, Sample, hash_t>>(ulongArray, runCount);
#endif

    benchmarkMap<tsl::robin_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<tsl::robin_pg_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<ska::flat_hash_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    /* TODO: benchmarkMap<ska::bytell_hash_map<Sample, Sample, hash_t>>(ulongArray, runCount); */
    benchmarkMap<robin_hood::unordered_flat_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<robin_hood::unordered_node_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<robin_hood::unordered_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<std::unordered_map<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<emhash5::HashMap<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<emhash6::HashMap<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<emhash7::HashMap<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<emilib2::HashMap<Sample, Sample, hash_t>>(ulongArray, runCount);
    benchmarkMap<emilib3::HashMap<Sample, Sample, hash_t>>(ulongArray, runCount);
#if HAVE_BOOST
    benchmarkMap<boost::unordered_flat_map<Sample, Sample, hash_t>>(ulongArray, runCount);
#endif

    cout << "\n# Ordered Maps:" << endl;
    benchmarkMap<std::map<Sample, Sample>>(ulongArray, runCount);
    benchmarkMap<phmap::btree_map<Sample, Sample>>(ulongArray, runCount);

    return 0;
}
