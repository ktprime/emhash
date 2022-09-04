#include "util.h"

#include "tsl/robin_map.h"
#include "martinus/robin_hood.h"
#include "phmap/phmap.h"
#include "hash_table7.hpp"
#include "hash_table6.hpp"
#include "hash_table5.hpp"
#include "hash_table8.hpp"
#include "emilib/emilib.hpp"
#include "emilib/emilib2.hpp"
#include "emilib/emilib2s.hpp"

#include "rigtorp/rigtorp.hpp"

#if CXX17
#include "martinus/unordered_dense.h"
#endif

#include <algorithm>
#include <numeric>

using namespace std;

static int max_n = (1 << 21) / 4 * 3;
static int max_trials = (1 << 26) / max_n;

struct stats {
    float average = 0.0f;
    float stdev = 0.0f;
    float percentile_95 = 0.0f;
    float percentile_99 = 0.0f;
    float percentile_999 = 0.0f;
};

stats get_statistics(vector<float> &v) {
    auto n = v.size();
    stats s;
    s.average = std::accumulate(v.begin(), v.end(), 0.0f) / n;
    float variance = 0.0f;
    for (auto value : v)
        variance += pow(value - s.average, 2.0f);

    variance /= n;
    s.stdev = sqrt(variance);
    std::sort(v.begin(), v.end());
    s.percentile_95 = *(v.begin() + (19 * n / 20));
    s.percentile_99 = *(v.begin() + (99 * n / 100));
    s.percentile_999 = *(v.begin() + (999 * n / 1000));
    return s;
}

template <typename HashTableType> void hash_table_test(const char* map)
{
    vector<float> durations_insert; vector<float> durations_find;
    vector<float> durations_miss;   vector<float> durations_erase;
    vector<float> durations_iter;
    vector<int64_t> v(max_n);

    mt19937 gen(max_n);
    for (auto& a : v) a = gen();

    for (int t = 0; t < max_trials; ++t)
    {
        HashTableType h; h.reserve(max_n);
        float duration = 0.0f;
        {
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            auto sum = 0;
            for (auto num : v)
                sum += h.emplace(num, 0).second;

            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<float, micro>>(end - start).count();
            durations_insert.push_back(duration);
        }

        {
            duration = 0.0f;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            auto sum = 0;
            for (auto num : v)
                sum += h.count(num);

            if (sum != v.size()) exit(0);

            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<float, micro>>(end - start) .count();
            durations_find.push_back(duration);
        }

        {
            duration = 0.0f;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            auto sum = 0;
            for (auto num : v)
                sum += h.count(num + 1);

            if (sum > v.size()) exit(0);

            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<float, micro>>(end - start) .count();
            durations_miss.push_back(duration);
        }

        {
            duration = 0.0f;
            auto start = chrono::steady_clock::now();
            auto sum = 0;
            for (auto num : v)
                sum += num & 1;

            assert(sum != h.size());
            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<float, micro>>(end - start).count() * 100000;
            durations_iter.push_back(duration);
        }

        {
            duration = 0.0f;
            shuffle(v.begin(), v.end()); auto start = chrono::steady_clock::now();
            auto sum = 0;
            for (auto num : v)
                sum += h.erase(num);

            assert(sum == h.size());
            auto end = chrono::steady_clock::now();
            duration += chrono::duration_cast<chrono::duration<float, micro>>(end - start).count();
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
            for (int i = len; i < 10; i++)
                putchar(' ');
        } else {
            for (int i = 0; i < 10; i++)
                putchar(map[i]);
        }

        printf("|Insert  |Find    |Miss    |Erase   |Iter    |\n");
        printf("|----------|--------|--------|--------|--------|--------|\n");
        printf("|Average   |");
        for (int i = 0; i < 5; i++) printf("%-7.f |", v[i].average / 100); printf("\n");

        printf("|Stdev%%    |");
        for (int i = 0; i < 5; i++) printf("%-7.2f%%|", 100.0 * v[i].stdev / v[i].average); printf("\n");

        if (max_trials >= 20) {
            printf("|95%%       |");
            for (int i = 0; i < 5; i++) printf("%-7.f |", v[i].percentile_95); printf("\n");
        }

        if (max_trials >= 100) {
            printf("|99%%       |");
            for (int i = 0; i < 5; i++) printf("%-7.f |", v[i].percentile_99); printf("\n");
        }
        if (max_trials >= 1000) {
            printf("|999%%      |");
            for (int i = 0; i < 5; i++) printf("%-7.f |", v[i].percentile_999); printf("\n");
        }
        printf("\n");
    }
}

int main(int argc, const char* argv[])
{
    if (argc > 1) {
        auto n = atoi(argv[1]);
        if (n > 10000)
            max_n = n;
        else
            max_n = max_n * n / 100;
    }
    else
        max_n += time(0) % 10024;

    if (argc > 2)
        max_trials = atoi(argv[2]);

    using ktype = int64_t;

#if TKey == 0
    using vtype = int64_t;
#else
    using vtype = int;
#endif

    printf("maxn = %d, loops = %d\n", max_n, max_trials);

#if FIB_HASH
    using QintHasher = Int64Hasher<ktype>;
#elif HOOD_HASH
    using QintHasher = robin_hood::hash<ktype>;
#else
    using QintHasher = std::hash<ktype>;
#endif

    hash_table_test<rigtorp::HashMap<ktype, vtype, QintHasher>>("rigtorp");
#ifdef CXX20
    hash_table_test<jg::dense_hash_map<ktype, vtype, QintHasher>>("jg_dense");
    if ((max_n >> 20) == 0)
        hash_table_test<fph::DynamicFphMap<ktype, vtype, fph::MixSeedHash<vtype>>>("fph_table");
#endif


    hash_table_test<robin_hood::unordered_map<ktype, vtype, QintHasher>>("martinus");
    hash_table_test<phmap::flat_hash_map<ktype, vtype, QintHasher>>("phmap_flat");
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
    hash_table_test<emilib::HashMap<ktype, vtype, QintHasher>>("emilib1");
#endif

    hash_table_test<emilib2::HashMap<ktype, vtype, QintHasher>>("emilib2");
    hash_table_test<emilib3::HashMap<ktype, vtype, QintHasher>>("emilib3");

#if ABSL
    hash_table_test<absl::flat_hash_map<ktype, vtype, QintHasher>>("absl_flat");
#endif

    hash_table_test<std::unordered_map<ktype, vtype, QintHasher>>("unordered_map");
    return 0;
}

