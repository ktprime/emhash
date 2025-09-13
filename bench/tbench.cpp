#include "util.h"

#include "tsl/robin_map.h"
#include "ska/flat_hash_map.hpp"
#include "martin/robin_hood.h"
#include "phmap/phmap.h"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table8.hpp"
#include "emilib/emilib2ss.hpp"
#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"

#include "rigtorp/rigtorp.hpp"

#if CXX17
#include "martin/unordered_dense.h"
#endif
#if HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#endif

#include <algorithm>
#include <numeric>

using namespace std;

static int max_n = (1 << 22) / 4 * 3;
static int max_trials = (1 << 26) / max_n;

struct stats {
    double average = 0;
    double stdev = 0;
    double percentile_95 = 0;
    double percentile_99 = 0;
    double percentile_999 = 0;
};

stats get_statistics(vector<double> &v) {
    std::sort(v.begin(), v.end());
    if (v.size() > 10) v.pop_back();
    if (v.size() > 100) v.pop_back();

    const auto n = v.size();
    stats s;
    s.average = std::accumulate(v.begin(), v.end(), 0) / n;

    double variance = 0;
    for (const auto value : v)
        variance += (value - s.average) * (value - s.average);

    variance /= n;
    s.stdev = sqrt(variance);
    s.percentile_95 = *(v.begin() + (19 * n / 20));
    s.percentile_99 = *(v.begin() + (99 * n / 100));
    s.percentile_999 = *(v.begin() + (999 * n / 1000));
    return s;
}

template <typename HashTableType> void hash_table_test(const char* map)
{
    vector<double> durations_insert; vector<double> durations_find;
    vector<double> durations_miss;   vector<double> durations_erase;
    vector<double> durations_iter;
    vector<int64_t> v(max_n);

    mt19937 gen(max_n);
    for (auto& a : v) a = gen();

    double load_factor = 1.0;
    for (int t = 0; t < max_trials; ++t)
    {
        HashTableType h; h.reserve(max_n / 8);
        double duration = 0;
        {
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            size_t sum = 0;
            for (auto num : v)
                sum += h.emplace(num, 0).second;

            load_factor = h.load_factor();
            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<double, micro>>(end - start).count();
            durations_insert.push_back(duration);
        }

        {
            duration = 0;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            size_t sum = 0;
            for (auto num : v)
                sum += h.count(num);

            if (sum != v.size()) exit(0);

            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<double, micro>>(end - start) .count();
            durations_find.push_back(duration);
        }

        {
            duration = 0;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            size_t sum = 0;
            for (auto num : v)
                sum += h.count(num + 1);

            if (sum > v.size()) exit(0);

            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<double, micro>>(end - start) .count();
            durations_miss.push_back(duration);
        }

        {
            duration = 0;
            auto start = chrono::steady_clock::now();
            size_t sum = 0;
            for (auto num : v)
                sum += num & 1;

            assert(sum != h.size());
            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<double, micro>>(end - start).count() * 100000;
            durations_iter.push_back(duration);
        }

        {
            duration = 0;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            size_t sum = 0;
            for (auto num : v)
                sum += h.erase(num);

            assert(sum == h.size());
            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<double, micro>>(end - start).count();
            durations_erase.push_back(duration);
        }

        h.clear();
    }

    {
        stats v[] = {
            get_statistics(durations_insert),
            get_statistics(durations_find),
            get_statistics(durations_miss),
            get_statistics(durations_erase),
            get_statistics(durations_iter)
        };

        int len = strlen(map);
        putchar('|');
        if (len < 10) {
            printf("%s", map);
            for (int i = len; i < 12; i++)
                putchar(' ');
        } else {
            for (int i = 0; i < 12; i++)
                putchar(map[i]);
        }

        printf("|Insert  |FHit    |FMiss   |Erase   |Iter    |\n");
        printf("|------------|--------|--------|--------|--------|--------|\n");
        printf("|Average     |");
        for (int i = 0; i < 5; i++) printf("%-7.lf |", v[i].average / 100); printf("lf = %.2lf\n", load_factor * 100);

        printf("|Stdev%%      |");
        for (int i = 0; i < 5; i++) printf("%-7.2lf%%|", 100.0 * v[i].stdev / v[i].average); printf("\n");

        if (max_trials >= 10) {
            printf("|95%%         |");
            for (int i = 0; i < 5; i++) printf("%-7.lf |", v[i].percentile_95 / 100); printf("\n");
        }

        if (max_trials >= 50) {
            printf("|99%%         |");
            for (int i = 0; i < 5; i++) printf("%-7.lf |", v[i].percentile_99 / 100); printf("\n");
        }
        if (max_trials >= 500) {
            printf("|999%%        |");
            for (int i = 0; i < 5; i++) printf("%-7.lf |", v[i].percentile_999 / 100); printf("\n");
        }
        printf("\n");
    }
}

int main(int argc, const char* argv[])
{
    if (argc > 1 && isdigit(argv[1][0])) {
        auto n = atoi(argv[1]);
        if (n > 10000)
            max_n = n;
        else
            max_n = max_n * n / 100;
    }
    else
        max_n += time(0) % 10024;

    if (argc > 2 && isdigit(argv[2][0]))
        max_trials = atoi(argv[2]);

    using ktype = int64_t;

#if TKey == 1
    using vtype = int64_t;
#else
    using vtype = int;
#endif

    printf("maxn = %d, loops = %d\n", max_n, max_trials);

#if FIB_HASH
    using QintHasher = Int64Hasher<ktype>;
#elif HOOD_HASH
    using QintHasher = robin_hood::hash<ktype>;
#elif STD_HASH
    using QintHasher = ankerl::unordered_dense::hash<ktype>;
#else
    using QintHasher = std::hash<ktype>;
#endif

    hash_table_test<rigtorp::HashMap<ktype, vtype, QintHasher>>("rigtorp");
#ifdef CXX20
    hash_table_test<jg::dense_hash_map<ktype, vtype, QintHasher>>("jg_dense");
    if ((max_n >> 20) == 0)
        hash_table_test<fph::DynamicFphMap<ktype, vtype, fph::MixSeedHash<vtype>>>("fph_table");
#endif


    hash_table_test<robin_hood::unordered_map<ktype, vtype, QintHasher>>("martin");
    hash_table_test<phmap::flat_hash_map<ktype, vtype, QintHasher>>("phmap_flat  ");
    hash_table_test<tsl::robin_map<ktype, vtype, QintHasher>>("tsl_robin_map");

    hash_table_test<emhash5::HashMap<ktype, vtype, QintHasher>>("emhash5");
    hash_table_test<emhash6::HashMap<ktype, vtype, QintHasher>>("emhash6");
    hash_table_test<emhash7::HashMap<ktype, vtype, QintHasher>>("emhash7");
    hash_table_test<emhash8::HashMap<ktype, vtype, QintHasher>>("emhash8");
#if CXX17
    hash_table_test<ankerl::unordered_dense::map<ktype, vtype, QintHasher>>("ankerl::dense");
#endif

#if ET > 1
    hash_table_test<ska::flat_hash_map<ktype, vtype>>("ska_flat");
#endif

#if HAVE_BOOST
    hash_table_test<boost::unordered_flat_map<ktype, vtype, QintHasher>>("boost::hflat");
#endif

    hash_table_test<emilib::HashMap<ktype, vtype, QintHasher>>("emilib1");
    hash_table_test<emilib2::HashMap<ktype, vtype, QintHasher>>("emilib2");
    hash_table_test<emilib3::HashMap<ktype, vtype, QintHasher>>("emilib3");

#if ABSL_HMAP
    hash_table_test<absl::flat_hash_map<ktype, vtype, QintHasher>>("absl_flat");
#endif

    hash_table_test<std::unordered_map<ktype, vtype, QintHasher>>("std::unormap");
    return 0;
}

