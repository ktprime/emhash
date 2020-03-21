#include "hash_table52.hpp"

#include <algorithm>
#include <random>
#include <set>
#include <unordered_map>
#include "lru_size.h"
#include "lru_map.h"

static long long getTime()
{
#if _WIN32 && 0
    FILETIME ptime[4] = {0};
    GetThreadTimes(GetCurrentThread(), &ptime[0], &ptime[1], &ptime[2], &ptime[3]);
    return (ptime[2].dwLowDateTime + ptime[3].dwLowDateTime) / 10000;
#elif 0
    struct rusage rup;
    getrusage(RUSAGE_SELF, &rup);
    long sec  = rup.ru_utime.tv_sec  + rup.ru_stime.tv_sec;
    long usec = rup.ru_utime.tv_usec + rup.ru_stime.tv_usec;
    return sec * 1000 + usec / 1000;
#elif _WIN32
    return clock();
#else
    return clock() / 1000;
#endif
}


int testHash(int argc, char* argv[])
{
    int n = argc > 1 ? atoi(argv[1]) : 5000*1000;
    printf("size = %d\n", n);

    {
        puts("1.one by one");
        {
            auto ts = getTime();
            emhash2::HashMap<int, int> emap(n);
            for (int i = 0; i < n; i++)
                emap.insert(i, 0);
            printf("emap insert time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            emhash2::HashMap<int, int> emap(n);
            for (int i = 0; i < n; i++)
                emap.insert_unique(i, 0);
            printf("emap insert time = %lld ms\n", getTime() - ts);
        }

        {
            auto ts = getTime();
            std::vector<int> v; v.reserve(n);
            for (int i = 0; i < n; i++)
                v.push_back(0);
            printf("vec time = %lld ms\n", getTime() - ts);
        }

        puts("\n");
    }

    {
        std::vector <int> data(n);
        for (int i = 0; i < n; i ++)
            data[i] = i;

        puts("2.random_shuffle");
        std::random_shuffle(data.begin(), data.end());
        {
            auto ts = getTime();
            emhash2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap time = %lld ms\n", getTime() - ts);
        }

        {
            auto ts = getTime();
            std::vector<int> v(n);
            for (auto d: data)
                v[d] = 0;
            printf("vec time = %lld ms\n", getTime() - ts);
        }
        puts("\n");
    }

    {
        std::vector <int> data(n);
        puts("3.random data");
        for (int i = 0; i < n; i ++)
            data[i] = (unsigned int)(rand() * rand() + rand()) % n;

        {
            auto ts = getTime();
            emhash2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            std::vector<int> v(n);
            for (auto d: data)
                v[d] = 1;
            printf("vec time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            std::set<int> sset;
            for (auto d: data)
                sset.insert(d);
            printf("set time = %lld ms\n", getTime() - ts);
        }
        puts("\n");
    }

    return 0;
}

int testLRU(int argc, char* argv[])
{
    int n = argc > 1 ? atoi(argv[1]) :1234567;
    printf("size = %d\n", n);

    std::vector <int> data(n);
    puts("3.random data");

    emlru_size::lru_cache<long, int> elru(n/2, n);
    LruMap<long, int> clru(n);

    std::mt19937_64 srng; srng.seed(rand());
    for (int i = 0; i < 10; i++)
    {
        for (int i = 0; i < n; i ++)
            data[i] = srng();
		//http://erdani.com/research/sea2017.pdf
        printf("loop %d\n", i + 1);
        {
            auto ts = getTime();
            for (auto v: data)
                elru.insert(v, 0);
            printf("    my insert time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                elru.count(v);
            printf("    my find hit time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                elru.count(v + 1);
            printf("    my find miss time = %lld ms\n", getTime() - ts);
        }
        puts("");
        {
            auto ts = getTime();
            for (auto v: data)
                clru.insert(v, 0);
            printf("    co insert time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                clru.find(v);
            printf("    co find   time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                clru.find(v + 1);
            printf("    co find miss time = %lld ms\n", getTime() - ts);
        }
        puts("    ============================");
    }

    return 0;
}

int main(int argc, char* argv[])
{
    testLRU(argc, argv);
    testHash(argc, argv);
    return 0;
}

