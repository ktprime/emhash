# emhash key feature
A very fast and efficient *open address based c++ flat hash map*, it can be bench/test it and compare the result with third party hash map. 

some features are not enabled by default and it also can be used by set the compile marco but may loss some tiny performance if necessary needed, some featue is conflicted each other or difficlut to merged and used in different hash table file. Not all feature can be open at same time in only one file.

- the default load factor is 0.8, also be set **1.0** by enable compile marco EMILIB_HIGH_LOAD (average 5% performance loss in hash_table3.hpp)

- **head only** support by c++0x/11/14/17 without any depency, interface is highly compatible with std::unordered_map, some new function is added for performance issiue if needed. for example _erase, shrink_to_fit, insert_unqiue, try_find

- At present from my 6 different benchmark(4 of them in bench dir), it's the **fastest** hash map for find performance(100% hit), and fast inserting performacne if no rehash(call reserve before inserting) and effficient erase.

- more **memory efficient** if the key.value size is not aligned than other's implemention *(size(key) % 8 != size(value) % 8)* 
for example hash_map<uint64_t, uint32_t> can save 1/3 memoery the hash_map<uint64_t, uint64_t>

- only one array allocted, a simple and **smart collision algorithm** with the collision element linked by array index like stl::unordered_map

- it can use a **second/backup hash** if the input hash is very bad with high collision by compile marco EMILIB_SAFE_HASH is set

- **lru** is also used if compile marco EMILIB_LRU_SET set for some special user case. for exmaple some key is "frequceny accessed", if the key is not in **main bucket** position, it'll be moved to main bucket from the tail to head and only  will be find only once during next time.

- can dump hash **collision statics** to analy, and choose different hash algorithm by set compile marco EMILIB_FIBONACCI_HASH or EMILIB_IDENTITY_HASH

- **no tombstones** is used in this hash map. performance will **not deteriorate** even high frequceny insertion and erasion
- more than **5 different** implementation to choose, each of them is some tiny different can be used in many case
for example some case pay attention on finding hot, some foucus on finding code(miss), and others care about insert or erase and somne on.

- many optimization with key is *integer*, some new feature is underdeveloing bfore statle to use.

- It's fully tested on OS(Win, Linux, Mac) with compiler(VS, clang, g++) and cpu(AMD, Intel, Arm).

# Example

```
static void basic_test(int n)
{
    printf("2. random_shuffle 1 - %d\n", n);
    {
        std::vector <int> data(n);
        for (int i = 0; i < n; i ++)
            data[i] = i;

        myrandom_shuffle(data.begin(), data.end());
        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap insert time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert_unique(v, 0); //assure key is not exist
            printf("emap unique time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::unordered_map<int, int> umap(n);
            for (auto v: data)
                umap.emplace(v, 0);
            printf("umap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, umap.load_factor());
        }

        {
            auto ts = now2ms();
            std::vector<int> v(n);
            for (auto d: data)
                v[d] = 0;
            printf("vec    time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            emilib9::HashSet<int> eset(n);
            for (auto v: data)
                eset.insert_unique(v);
            printf("eset unique time = %ld ms\n", now2ms() - ts);
        }

        {
            auto ts = now2ms();
            std::unordered_set<int> eset(n);
            for (auto v: data)
                eset.insert(v);
            printf("uset insert time = %ld ms\n", now2ms() - ts);
        }
        puts("\n");
    }

    puts("3. random data");
    {
        std::vector <int> data(n);
        for (int i = 0; i < n; i ++)
            data[i] = (uint32_t)(rand() * rand()) % n + rand();
        myrandom_shuffle(data.begin(), data.end());

        {
            auto ts = now2ms();
            emilib2::HashMap<int, int> emap(n);
            for (auto v: data)
                emap.insert(v, 0);
            printf("emap insert time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, emap.load_factor(), emap.size());
        }

        {
            auto ts = now2ms();
            std::unordered_map<int, int> umap(n);
            for (auto v: data)
                umap.emplace(v, 0);
            printf("umap insert time = %ld ms loadf = %.3lf\n", now2ms() - ts, umap.load_factor());
        }


        {
            auto ts = now2ms();
            emilib9::HashSet<int> eset(n);
            eset.insert(data.begin(), data.end());
            printf("eset insert range time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, eset.load_factor(), eset.size());
        }

        {
            auto ts = now2ms();
            std::unordered_set<int> uset(n);
            uset.insert(data.begin(), data.end());
            printf("uset insert time = %ld ms loadf = %.3lf/size %zd\n", now2ms() - ts, uset.load_factor(), uset.size());
        }

        puts("\n");
    }
}
```


# insert result
the simple benchmark (code in bench/martin_bench.cpp) compraed with std::unordered_map/std::unordered_set
##1 random_shuffle 1 - 12345678
* emap insert time = 440 ms
* emap unique time = 312 ms
* umap insert time = 1628 ms loadf = 0.942
* vec    time = 296 ms
* eset unique time = 220 ms
* uset insert time = 1728 ms

##2. random data
*    emap insert time = 568 ms loadf = 0.7
*    umap insert time = 2912 ms loadf = 0.939
*    eset insert range time = 1412 ms loadf = 0.734
*    uset insert time = 2916 ms loadf = 0.939

# some bad
- it's not node based hash map, so it can't keep the reference stable if rehash happens, use pointer or choose node base hash map.

- rehash/iteration performance is some slower than other robin-hood based hash implementation

- on some platform it'll be hanged compiled by some g++ on some system, compile with flag **-fno-stirct-aliasing**, it'll be fixed soon
- for very large key.value, use pointer instead of value if your care about memory usage and insertion and copy opertion is very frequcney.

- some bucket function is support just like stl do. load factor is less than 1.0.

- the only known bug as follow. if erase not current key or iterater during iteration without break. some key will iteraored twice or missed.

```
    HashMap<int,int> myhash;
    int key = some_key;
    //dome some init
    for (auto it : myhash)
    {
        if (key = it.first) {
            myhash.erase(key);  //no any break
       }
       do_some_more();
    }
```
# futrue work to improve

