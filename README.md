# fast and memory efficient *open addressing c++ flat hash table/map*
 

    some feature is not enabled by default and it also can be used by set the compile marco but may loss tiny performance, some featue is conflicted each other or difficlut to be merged into only one head file and so it's distributed in different hash table file. Not all feature can be open in only one file(one hash map).

third party bechmark from https://martin.ankerl.com/2022/08/27/hashmap-bench-01/ and https://jacksonallan.github.io/c_cpp_hash_tables_benchmark/


- **load factor** can be set **0.999** by set marco *EMHASH_HIGH_LOAD == somevalue* (in hash_table[5-8].hpp)

- **head only** support by c++11/14/17/20, interface is highly compatible with std::unordered_map, some new functions added for performance.
    - _erase : return void after erasion
    - shrink_to_fit : shrink fit for saving memory
    - insert_unqiue : insert unique key without finding
    - try_find : return value
    - set_get : once find/insert combined

- **efficient** than other's hash map if key&value is not aligned (sizeof(key) % 8 != sizeof(value) % 8), hash_map<uint64_t, uint32_t> can save 1/3 memoery than hash_map<uint64_t, uint64_t>.

- **lru** marco EMHASH_LRU_SET set. some keys is "frequceny accessed", if keys are not in **main bucket** slot, it'll be swaped with main bucket, and will be probed once.

- **no tombstones**. performance will **not deteriorate** even high frequceny of insertion & erasion.

- **4 different** implementation, for example some case pay attention on finding hot, some focus on finding cold(miss), and others only care about insert or erase and so on.

- **find hit** is fastest at present, fast inserting(**reserve**) and effficient erasion from 6 different benchmarks(4 of them in my bench dir) by my bench

- fully tested on OS(Win, Linux, Mac) with compiler(msvs, clang, gcc) and cpu(AMD, Intel, ARM64).

- many optimization on **integer** key.

# emhash design

- **one array&inline entries** node/entry contains a struct(Key key, size_t bucket, Value value) without separate footprint

- **main bucket** equal to key_hash(key) % size, can not be occupyed(like cockoo hash) and many opertions serarch from it

- **smart collision resolution**, collision node is linked (bucket) like separate channing.
it's not suffered heavily performance loss by primary and secondary clustering.

- **3-way combined** probing used to seach empty slot.
   - linear probing search 2-3 cpu cachelines
   - quadratic probing works after limited linear probing
   - linear search both begin&end with last founded empty slot

- a new linear probing is used (in hash_table5.hpp).
    normaly linear probing is inefficient with high load factor, it use a new 3-way linear
probing strategy to search empty slot. from benchmark even the load factor > 0.9, it's more 2-3 timer fast than traditional seach strategy.

- **second/backup hashing function** if the input hash is bad with a very high collision if the compile marco *EMHASH_SAFE_HASH* is set to defend hash attack(but 10% performance descrease)

- dump hash **collision statics** to analyze cache performance, number of probes for look up of successful/unsuccessful can be showed from dump info.

- finding **64 slots** once using x86 instruction bit scanf(ctz).

- choose *different* hash algorithm by set compile marco *EMHASH_FIBONACCI_HASH* or *EMHASH_IDENTITY_HASH* depend on use case.

- A thirdy party string hash algorithm is used for string key [wyhash](https://github.com/wangyi-fudan/wyhash), which is faster than std::hash implementation

# example

```cpp
        // constructor
        emhash5::HashMap<int, int> m1(4);
        m1.reserve(100);
        for (int i = 1; i < 100; i++)
          m1.emplace_unique(i, i); //key must be unique, performance is better than emplace, operator[].
       
        auto no_value = m1.at(0); //no_value = 0; no exception throw!!!. only return zero for integer value.

        // list constructor
        emhash5::HashMap<int, std::string> m2 = {
            {1, "foo"},
            {3, "bar"},
            {2, "baz"},
        };
   
        auto* pvalue = m2.try_get(1); //return nullptr if key is not exist
        if (m2.try_set(4, "for"))   printf("set success");
        if (!m2.try_set(1, "new"))  printf("set failed");        
        string ovalue = m2.set_get("1", "new"); //ovalue = "foo" and m2[1] == "new"

        for(auto& p: m2)
           std::cout << " " << p.first << " => " << p.second << '\n';

        // copy constructor
        emhash5::HashMap<int, std::string> m3 = m2;
        // move constructor
        emhash5::HashMap<int, std::string> m4 = std::move(m2);
        
        //insert. insert_unique. emplace
        m2.insert_unique(4, "four");
        m2[4] = "four_again";
        m2.emplace(std::make_pair(4, "four"));
        m2.insert({{6, "six"}, {5, "five"}});

        // range constructor
        std::vector<std::pair<std::bitset<8>, int>> v = { {0x12, 1}, {0x01,-1} };
        emhash5::HashMap<std::bitset<8>, double> m5(v.begin(), v.end());

        //Option 1 for a constructor with a custom Key type
        //Define the KeyHash and KeyEqual structs and use them in the template
        emhash8::HashMap<Key, std::string, YourKeyHash, YourKeyEqual> m6 = {
            { {"John", "Doe"}, "example"},
            { {"Mary", "Sue"}, "another"}
        };

        //Option 2 for a constructor with a custom Key type
        // Define a const == operator for the class/struct and specialize std::hash
        // structure in the std namespace
        emhash7::HashMap<Foo, std::string> m7 = {
            { Foo(1), "One"}, { 2, "Two"}, { 3, "Three"}
        };

#if CXX20
        struct Goo {int val; };
        auto hash = [](const Goo &g){ return std::hash<int>{}(g.val); };
        auto comp = [](const Goo &l, const Goo &r){ return l.val == r.val; };
        emhash5::HashMap<Goo, double, decltype(hash), decltype(comp)> m8;
#endif

        emhash8::HashMap<int,char> m8 = {{1,'a'},{2,'b'}};
        for(const auto [k, v] : m8}) 
            printf("%d %c\n", k, v);

        const auto* data = m8.values();//order by insert
        for (int i = 0; i < m8.size(); i++)
            printf("%d %c\n", data[i].first, data[i].second);

```

# only emhash can 
   The following benchmark shows that emhash has amazing performance even insert & erase with a high load factor 0.999.
```cpp
static void RunHighLoadFactor()
{
    std::random_device rd;
    const auto rand_key = rd();
#if 1
    WyRand srngi(rand_key), srnge(rand_key);
#else
    std::mt19937_64 srngi(rand_key), srnge(rand_key);
#endif

    const auto max_lf   = 0.999f; //<= 0.9999f
    const auto vsize    = 1u << (20 + rand_key % 6);//must be power of 2
    emhash7::HashMap<int64_t, int> myhash(vsize, max_lf);

    auto nowus = getus();
    for (size_t i = 0; i < size_t(vsize * max_lf); i++)
        myhash.emplace(srngi(), i);
    assert(myhash.bucket_count() == vsize); //no rehash

    const auto insert_time = getus() - nowus; nowus = getus();
    //erase & insert at a fixed load factor
    for (size_t i = 0; i < vsize; i++) {
        myhash.erase(srnge()); //erase an old key
        myhash[srngi()] = 1;   //insert a new key
    }
    const auto erase_time = getus() - nowus;
    printf("vsize = %d, load factor = %.4f, insert/erase = %ld/%ld ms\n",
        vsize, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
    assert(myhash.load_factor() >= max_lf - 0.001);
}
```
```
  vsize = 1048576, load factor = 0.9990, insert/erase = 25/76 ms
  vsize = 2097152, load factor = 0.9990, insert/erase = 52/222 ms
  vsize = 4194304, load factor = 0.9990, insert/erase = 117/450 ms
  vsize = 8388608, load factor = 0.9990, insert/erase = 251/1009 ms
```
### benchmark

some of benchmark result is uploaded, I use other hash map (martinus, ska, phmap, dense_hash_map ...) source to compile and benchmark.
[![Bench All](https://github.com/ktprime/emhash/blob/master/bench/em_bench.cpp)] and [![Bench High Load](https://github.com/ktprime/emhash/blob/master/bench/martin_bench.cpp)]

another html result with impressive curve [chartsAll.html](https://github.com/ktprime/emhash/blob/master/bench/tsl_bench/chartsAll.html)
(download all js file in tls_bench dir)
generated by [Tessil](https://tessil.github.io/2016/08/29/benchmark-hopscotch-map.html) benchmark code

txt file result [martin_bench.txt](https://github.com/ktprime/emhash/blob/master/bench/martin_bench.txt) generated by code from
[martin](https://github.com/martinus/map_benchmark)

the benchmark code is some tiny changed for injecting new hash map, the result is not final beacuse it depends on os, cpu, compiler and dataset input.

my result is benched on 3 linux server(amd, intel, arm64), win10 pc/Laptop and apple m1): low is best
![](int64_t_int64_t.png)
![](int64_t_int64_t_m1.png)
![](int_string.png)
![](string_string.png)
![](Struct_int64_t.png)
![](int64_t_Struct.png)
![](int64_t.png)

# some bad
- it's not a node-based hash map and can't keep the reference stable if insert/erase/rehash happens, use value pointer or choose the other node base hash map.
```
    emhash7:HashMap<int,int> myhash(10);
    myhash[1] = 1;
    auto& myref = myhash[1];//**wrong used here**,  can not keep reference stable
     ....
    auto old = myref ;  // myref maybe be changed and not invalid.

    emhash7:HashMap<int,int> myhash2;
    for (int i = 0; i < 10000; i ++)
        myhash2[rand()] = myhash2[rand()]; // it will be crashed because of rehash, call reserve before or use insert.
 ```

- for very large key-value, use pointer instead of value if you care about memory usage with high frequency of insertion or erasion
```
  emhash7:HashMap<keyT,valueT> myhash; //value is very big, ex sizeof(value) 100 byte

  emhash7:HashMap<keyT,*valueT> myhash2; //new valueT, or use std::shared_ptr<valueT>.

```

- the only known bug as follow example, if erase key/iterator during iteration. one key will be iteraored twice or missed. and fix it can desearse performance 20% or even much more and no good way to fix.

```
    emhash7:HashMap<int,int> myhash;
    //dome some init ...
    for (const auto& it : myhash)
    {
        if (some_key == it.first) {
            myhash.erase(key);  //no any break
       }
       ...
       do_some_more();
    }
    
    //change upper code as follow
    for (auto it = myhash.begin(); it != myhash.end(); it++)
    {
        if (some_key == it.first) {
            it = myhash.erase(it);
       }
       ...
       do_some_more();
    }
    
```

```
     emhash7:HashMap<int,int> myhash = {{1,2},{5,2},};
     auto it = myhash.find(1);
    
     it = map.erase( it );
     map.erase( it++ );// it's error code. use upper line
```

# Which one?

So what’s actually the best map to choose? it depends.  Here are some basic suggestion you can follow:

 1. if key/value is complex/big(ex std::string or user defined struct), try to use emhash8
because node's momory is continuous like std::vector, very fast iteration speed, search & insert is very fast and low memory usage,but it's some slow with deletion compared with emhash5-7.

 2. as for insertion performance emhash7 is a good choice, it  can set a extreme high load factor(>0.90, 0.99 is also ok) for memory usage.

 3. emhash5/6 is optimized for find hit/erasion with integer keys, it also can be used as a small stack hashmap if set marco EMH_SMALL_SIZE.

The follow result is benchmarked on AMD 5800h cpu on windows 10/gcc 11.3(
code https://github.com/ktprime/emhash/blob/master/bench/qbench.cpp)


|10       hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  32.6| 26.0| 27.7| 35.6| 23.8| 62.5     |
|emhash8::HashMap|  46.5| 27.5| 29.5| 29.2| 23.2| 62.5     |
|emhash7::HashMap|  38.0| 27.6| 29.1| 27.9| 23.9| 62.5     |
|emhash6::HashMap|  38.9| 27.2| 28.8| 28.1| 23.9| 62.5     |
|emhash5::HashMap|  41.3| 27.2| 28.2| 27.8| 28.7| 62.5     |


|200      hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  12.4| 3.19| 7.64| 9.56| 2.39| 78.1     |
|emhash8::HashMap|  15.0| 5.14| 6.40| 7.22| 1.17| 78.1     |
|emhash7::HashMap|  12.4| 5.09| 5.92| 4.95| 1.88| 78.1     |
|emhash6::HashMap|  12.8| 4.96| 6.77| 5.67| 1.89| 78.1     |
|emhash5::HashMap|  15.8| 4.88| 5.79| 5.43| 3.54| 78.1     |


|3000     hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  9.67| 1.90| 4.98| 7.16| 1.33| 73.2     |
|emhash8::HashMap|  13.5| 4.00| 5.38| 6.21| 0.08| 73.2     |
|emhash7::HashMap|  10.6| 3.98| 4.88| 3.93| 0.76| 73.2     |
|emhash6::HashMap|  11.4| 3.81| 5.70| 4.78| 0.76| 73.2     |
|emhash5::HashMap|  14.3| 3.82| 4.80| 4.50| 2.94| 73.2     |


|40000    hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  10.7| 2.82| 2.85| 8.47| 1.43| 61.0     |
|emhash8::HashMap|  16.0| 4.22| 6.01| 7.14| 0.00| 61.0     |
|emhash7::HashMap|  11.7| 4.19| 5.47| 4.44| 0.73| 61.0     |
|emhash6::HashMap|  13.0| 3.89| 5.86| 5.24| 0.73| 61.0     |
|emhash5::HashMap|  15.7| 3.93| 5.30| 5.01| 4.40| 61.0     |


|500000   hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  20.0| 13.3| 3.99| 17.9| 1.58| 47.7     |
|emhash8::HashMap|  26.2| 6.39| 7.62| 9.17| 0.00| 47.7     |
|emhash7::HashMap|  18.3| 10.6| 7.79| 9.82| 0.78| 47.7     |
|emhash6::HashMap|  21.6| 8.74| 8.39| 9.82| 0.79| 47.7     |
|emhash5::HashMap|  24.5| 8.19| 9.93| 8.93| 6.16| 47.7     |


|7200000  hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  50.3| 21.3| 24.8| 29.6| 0.97| 85.8     |
|emhash8::HashMap|  82.4| 18.5| 18.7| 39.2| 0.00| 85.8     |
|emhash7::HashMap|  51.2| 18.4| 21.1| 20.2| 0.63| 85.8     |
|emhash6::HashMap|  53.5| 16.7| 19.2| 26.0| 0.63| 85.8     |
|emhash5::HashMap|  62.6| 16.0| 19.7| 22.6| 1.60| 85.8     |


|10000000 hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  79.4| 32.3| 17.7| 48.0| 1.45| 59.6     |
|emhash8::HashMap|  95.6| 18.3| 18.1| 46.3| 0.00| 59.6     |
|emhash7::HashMap|  66.0| 16.9| 18.1| 20.4| 0.74| 59.6     |
|emhash6::HashMap|  63.6| 15.1| 17.0| 24.0| 0.75| 59.6     |
|emhash5::HashMap|  64.7| 16.6| 18.8| 22.3| 4.75| 59.6     |


|50000000 hashmap|Insert|Fhit |Fmiss|Erase|Iter |LoadFactor|
|----------------|------|-----|-----|-----|-----|----------|
|emilib2::HashMap|  79.4| 31.2| 27.7| 55.1| 1.22| 74.5     |
|emhash8::HashMap|  93.1| 19.4| 20.1| 41.7| 0.00| 74.5     |
|emhash7::HashMap|  62.7| 20.3| 22.4| 25.2| 0.68| 74.5     |
|emhash6::HashMap|  61.5| 16.1| 18.0| 28.0| 0.67| 74.5     |
|emhash5::HashMap|  65.1| 16.1| 18.8| 24.3| 2.72| 74.5     |


