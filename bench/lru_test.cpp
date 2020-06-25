#include <algorithm>
#include <random>
#include <set>
#include <unordered_map>

//#define EMH__REHASH_LOG 123457
//#define EMH__LRU_TIME      10
//#define EMH__SAFE_REMOVE   1

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

int testLRU(int argc, char* argv[])
{
    int n = argc > 1 ? atoi(argv[1]) : (rand() * rand()) % 1234567 + 123456;
    printf("size = %d\n", n);

    std::vector <int> data;
    puts("3.random data");

    emlru_size::lru_cache<int, int> elru(n/3, n / 2);
    LruMap<int, int> clru(n);

    data.reserve(8*n);
    std::mt19937_64 srng; srng.seed(rand());
    for (int i = 0; i < 8; i++)
    {
        for (int i = 0; i < n; i ++)
            data.push_back(srng());

        printf("loop %d size %zd\n", i + 1, data.size());
        {
            auto ts = getTime();
            for (auto v: data)
                elru.insert(v, 0);
            printf("    my insert time = %lld ms\n", getTime() - ts);
        }
        if (1)
        {
            auto ts = getTime();
            for (int i = 1; i <= n; i ++) {
                auto v = data[srng() % i];
                elru.count(v);
            }
            printf("    my find hit time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                elru.count(v + v % 2);
            printf("    my find half time = %lld ms\n", getTime() - ts);
        }

        puts("");
        {
            auto ts = getTime();
            for (auto v: data)
                clru.insert(v, 0);
            printf("    co insert time = %lld ms\n", getTime() - ts);
        }
        if (1)
        {
            auto ts = getTime();
            for (int i = 1; i <= n; i ++) {
                auto v = data[srng() % i];
                clru.find(v);
            }
            printf("    co find   time = %lld ms\n", getTime() - ts);
        }
        {
            auto ts = getTime();
            for (auto v: data)
                clru.find(v + v % 2);
            printf("    co find half time = %lld ms\n", getTime() - ts);
        }
        puts("    ============================");
    }

    return 0;
}

int main(int argc, char* argv[])
{
    srand(time(0));
    testLRU(argc, argv);
    return 0;
}

