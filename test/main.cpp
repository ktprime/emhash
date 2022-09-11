#ifndef _WIN32
//#define BOOST_TEST_MODULE hash_map_tests
//#include <boost/test/unit_test.hpp>
//#include "utils.h"
#endif


#define EMH_IDENTITY_HASH 1

//#include "wyhash.h"
#include "eutil.h"

#define EMH_ITER_SAFE 1
#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"
#include "emilib/emilib2.hpp"


#include "martinus/robin_hood.h"    //https://github.com/martin/robin-hood-hashing/blob/master/src/include/robin_hood.h
#include "phmap/phmap.h"          //https://github.com/greg7mdp/parallel-hashmap/tree/master/parallel_hashmap

#if CXX20
#include <string_view>
#include "wyhash.h"
struct string_hash
{
    using is_transparent = void;
#if WYHASH_LITTLE_ENDIAN
    std::size_t operator()(const char* key)             const { auto ksize = std::strlen(key); return wyhash(key, ksize, ksize); }
    std::size_t operator()(const std::string& key)      const { return wyhash(key.data(), key.size(), key.size()); }
    std::size_t operator()(const std::string_view& key) const { return wyhash(key.data(), key.size(), key.size()); }
#else
    std::size_t operator()(const char* key)             const { return std::hash<std::string_view>()(std::string_view(key, strlen(key))); }
    std::size_t operator()(const std::string& key)      const { return std::hash<std::string>()(key); }
    std::size_t operator()(const std::string_view& key) const { return std::hash<std::string_view>()(key); }
#endif
};

struct string_equal
{
    using is_transparent = int;

#if 1
    bool operator()(const std::string_view& lhs, const std::string& rhs) const {
        return lhs.size() == rhs.size() && lhs == rhs;
    }
#endif

    bool operator()(const char* lhs, const std::string& rhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }

    bool operator()(const char* lhs, const std::string_view& rhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }

    bool operator()(const std::string_view& rhs, const char* lhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }
};

static int find_strview_test()
{
    emhash6::HashMap<const std::string, char, string_hash, string_equal> map;
    std::string_view key = "key";
    std::string     skey = "key";
    map.emplace(key, 0);
    const auto it = map.find(key); // fail
    assert(it == map.find("key"));
    assert(it == map.find(skey));

    assert(key == "key");
    assert(key == skey);
    return 0;
}
#endif

struct Key {
    std::string first;
    std::string second;
};

struct KeyHash {
    std::size_t operator()(const Key& k) const
    {
        return std::hash<std::string>()(k.first) ^
            (std::hash<std::string>()(k.second) << 1);
    }
};

struct KeyEqual {
    bool operator()(const Key& lhs, const Key& rhs) const
    {
        return lhs.first == rhs.first && lhs.second == rhs.second;
    }
};

struct Foo {
    Foo(int val_) : val(val_) {}
    int val;
    bool operator==(const Foo &rhs) const { return val == rhs.val; }
};

namespace std {
    template<> struct hash<Foo> {
        std::size_t operator()(const Foo &f) const {
            return std::hash<int>{}(f.val);
        }
    };
}

// print out a std::pair
template <class Os, class U, class V>
Os& operator<<(Os& os, const std::pair<U,V>& p) {
    return os << '{' << p.first << ", " << p.second << '}';
}

template<typename Os, typename Container>
inline Os& operator<<(Os& os, Container const& cont)
{
    os << "{";
    for (const auto& item : cont) {
        os << "{" << item.first << ", " << item.second << "}";
    }
    return os << "}" << std::endl;
}

#if 0
#define ehmap  emilib2::HashMap
#else
#define ehmap  emhash7::HashMap
#endif
#define ehmap5 emhash5::HashMap
#define ehmap6 emhash6::HashMap
#define ehmap7 emhash7::HashMap
#define ehmap8 emhash8::HashMap

static void TestApi()
{
    printf("============================== %s ============================\n", __FUNCTION__);

    {
        // default constructor: empty map
        ehmap<std::string, std::string> m1;
        assert(m1.count("1") == 0);

        // list constructor
        ehmap<int, std::string> m2 =
        {
            {1, "foo"},
            {3, "bar"},
            {2, "baz"},
        };

        m2[2] = "frist";
        m2.find(2)->second = "second";
        m2.insert({3, "null"}).first->second = "third";
        m2.emplace(4, "null").first->second = "four";
        for (auto& it : m2)
            printf("%d -> %s\n", it.first, it.second.data());

        // copy constructor
        ehmap<int, std::string> m3 = m2;

        ehmap<int, int> m21 =
        {
            {2, 2}, {4, 3},
        };

        m21.clear();
        m21.emplace(1,1);
        m21.emplace(3,1);
        for (int i = 0; i < 16; i++)
            m21[i] = 0;
        assert(m21.size() == 16);


        // move constructor
        auto m4 = std::move(m2);

        //copy constructor
        m2 = std::move(m4);

        assert(m4.size() == 0);
        assert(m4.count(1) == 0);
        assert(m4.cbegin() == m4.end());

        m4[1] = "cdd";
        m4.clear();  m4.clear();
        assert(m4.size() == 0);

        m4[2] = "2";  m4.emplace(2, "22"); m4[3] = "3";
        assert(m4.size() == 2 && m4[2] == "2");

        m4.erase(2); assert(0 == m4.erase(2));
        assert(m4.size() == 1);

        m4.erase(m4.find(3));
        assert(m4.size()== 0);

        m4.clear();

        // range constructor
        std::vector<std::pair<std::bitset<8>, int>> v = { {0x12, 1}, {0x01,-1} };
        ehmap<std::bitset<8>, double> m5(v.begin(), v.end());

        //Option 1 for a constructor with a custom Key type
        // Define the KeyHash and KeyEqual structs and use them in the template
        ehmap<Key, std::string, KeyHash, KeyEqual> m6 = {
            { {"John", "Doe"}, "example"},
            { {"Mary", "Sue"}, "another"}
        };

        //Option 2 for a constructor with a custom Key type
        // Define a const == operator for the class/struct and specialize std::hash
        // structure in the std namespace
        ehmap<Foo, std::string> m7 = {
            { Foo(1), "One"}, { 2, "Two"}, { 3, "Three"}
        };

#if CXX20
        struct Goo {int val; };
        auto hash = [](const Goo &g){ return std::hash<int>{}(g.val); };
        auto comp = [](const Goo &l, const Goo &r){ return l.val == r.val; };
        ehmap<Goo, double, decltype(hash), decltype(comp)> m8;
#endif

        ehmap<int,char> example = {{1,'a'},{2,'b'}};
        for(int x: {2, 5}) {
            if(example.contains(x)) {
                std::cout << x << ": Found\n";
            } else {
                std::cout << x << ": Not found\n";
            }
        }
    }

    {
        ehmap<int, std::string> dict = {{1, "one"}, {2, "two"}};
        assert(dict.insert({3, "three"}).second);

        dict.insert({4, "four"});
        dict.insert(std::make_pair(4, "four"));
        dict.insert({{4, "another four"}, {5, "five"}});

        bool ok = dict.insert({1, "another one"}).second;
        std::cout << "inserting 1 -> \"another one\" "
            << (ok ? "succeeded" : "failed") << '\n';

        std::cout << "contents:\n";
        for(auto& p: dict)
            std::cout << " " << p.first << " => " << p.second << '\n';

        std::cout << "contents2:\n";
        ehmap<int, std::string> dict2;
        dict2.insert(dict.begin(), dict.end());
        for(auto& p: dict2)
            std::cout << " " << p.first << " => " << p.second << '\n';
    }

    {
        ehmap<std::string, std::string> m;
        // uses pair's move constructor
        m.emplace(std::make_pair(std::string("a"), std::string("a")));
        // uses pair's converting move constructor
        m.emplace(std::make_pair("b", "abcd"));
        m.emplace(std::move(std::make_pair("b", "abcd")));
        // uses pair's template constructor
        m.emplace("d", "ddd");

#if 0
        // uses pair's piecewise constructor
        m.emplace(std::piecewise_construct,
        std::forward_as_tuple("c"),
        std::forward_as_tuple(10, 'c'));
        // as of C++17, m.try_emplace("c", 10, 'c'); can be used
#endif
        for (const auto& p : m) {
            std::cout << p.first << " => " << p.second << '\n';
        }
    }

    {
        auto print_node = [&](const auto &node) {
            std::cout << "[" << node.first << "] = " << node.second << '\n';
        };

        auto print_result = [&](auto const &pair) {
            std::cout << (pair.second ? "inserted: " : "assigned: ");
            print_node(*pair.first);
        };

        ehmap<std::string, std::string> myMap;
        print_result( myMap.insert_or_assign("a", "apple"     ) );
        print_result( myMap.insert_or_assign("b", "banana"    ) );
        print_result( myMap.insert_or_assign("c", "cherry"    ) );
        print_result( myMap.insert_or_assign("c", "clementine") );
        for (const auto &node : myMap) { print_node(node); }
    }

    {
        auto print = [](auto const comment, auto const& map) {
            std::cout << comment << "{";
            for (const auto &pair : map) {
                std::cout << "{" << pair.first << ": " << pair.second << "}";
            }
            std::cout << "}\n";
        };

        ehmap<char, int> letter_counts {{'a', 27}, {'b', 3}, {'c', 1}};

        print("letter_counts initially contains: ", letter_counts);

        letter_counts['b'] = 42;  // updates an existing value
        letter_counts['x'] = 9;  // inserts a new value

        print("after modifications it contains: ", letter_counts);

        // count the number of occurrences of each word
        // (the first call to operator[] initialized the counter with zero)
        ehmap<std::string, int>  word_map;
        for (const auto &w : { "this", "sentence", "is", "not", "a", "sentence",
                "this", "sentence", "is", "a", "hoax"}) {
            ++word_map[w];
        }
        word_map["that"]; // just inserts the pair {"that", 0}

        //for (const auto [word, id, count] : word_map) { std::cout << count << " occurrences of word '" << word << "'\n"; }
        for (const auto &it : word_map) { std::cout << it.second << " occurrences of word '" << it.first << "'\n"; }
    }

    {
        ehmap<int, std::string, std::hash<int>> c = {
            {1, "one" }, {2, "two" }, {3, "three"},
            {4, "four"}, {5, "five"}, {6, "six"  }
        };

        // erase all odd numbers from c
        for(auto it = c.begin(); it != c.end(); ) {
            printf("%d:%s\n", it->first,  it->second.data());
            if(it->first % 2 != 0)
                it = c.erase(it);
            else
                ++it;
        }

        for(auto& p : c) {
            std::cout << p.second << ' ';
        }
    }

    {
        ehmap8<int, char> container{{1, 'x'}, {2, 'y'}, {3, 'z'}, {4, 'z'}};
//        using vtype = ehmap<int, char>::value_pair;
        using vtype = ehmap<int, char>::value_type;
        auto print = [](vtype& n) {
            std::cout << " " << n.first << '(' << n.second << ')';
        };

        std::cout << "Before clear:";
        std::for_each(container.begin(), container.end(), print);
        std::cout << "\nSize=" << container.size() << '\n';

        std::cout << "Clear\n";
        container.clear();

        std::cout << "After clear:";
        std::for_each(container.begin(), container.end(), print);
        std::cout << "\nSize=" << container.size() << '\n';
    }

    //erase(first, last)
    {
        ehmap8<int, char> container{{1, 'x'}, {2, 'y'}, {3, 'z'}, {4, 'z'}};
        {
            auto n1 = container;
            n1.erase(n1.find(1), n1.find(4));
            assert(n1.size() == 1);

            n1 = container;
            n1.erase(n1.find(5), n1.find(1));
            assert(n1.size() == container.size());

            auto n3 = container;
            n3.erase(n3.find(2), n3.find(2));
            assert(n3.size() == container.size());
        }

        {
            auto n2 = container;
            n2.erase(n2.find(1), n2.find(5));
            assert(n2.size() == 0);

            n2 = container;
            n2.erase(n2.find(2), n2.find(5));
            assert(n2.size() == 1);
        }

        {
            auto n3 = container;
            n3.erase(n3.find(2), n3.find(4));
            assert(n3.size() == 2);
        }
    }

    struct Node { double x, y; };

    {
        Node nodes[3] = { {1, 0}, {2, 0}, {3, 0} };

        //mag is a map mapping the address of a Node to its magnitude in the plane
        ehmap<Node*, double> mag = {
            { nodes,     1 },
            { nodes + 1, 2 },
            { nodes + 2, 3 },
        };

        mag.reserve(6);
        //Change each y-coordinate from 0 to the magnitude
        for(auto iter = mag.begin(); iter != mag.end(); ++iter){
            auto cur = iter->first; // pointer to Node
            cur->y = iter->second; // could also have used  cur->y = iter->second;
        }

        //Update and print the magnitude of each node
        for(auto iter = mag.begin(); iter != mag.end(); ++iter){
            auto cur = iter->first;
            mag[cur] = std::hypot(cur->x, cur->y);
            std::cout << "The magnitude of (" << cur->x << ", " << cur->y << ") is ";
            std::cout << iter->second << '\n';
        }

        //Repeat the above with the range-based for loop
        for(auto i : mag) {
            auto cur = i.first;
            cur->y = i.second;
            mag[cur] = std::hypot(cur->x, cur->y);
            std::cout << "The magnitude of (" << cur->x << ", " << cur->y << ") is ";
            std::cout << mag[cur] << '\n';
            //Note that in contrast to std::cout << iter->second << '\n'; above,
            // std::cout << i.second << '\n'; will NOT print the updated magnitude
        }
    }

    {
        Node nodes[3] = { {1, 0}, {2, 0}, {3, 0} };

        //mag is a map mapping the address of a Node to its magnitude in the plane
        ehmap<Node *, double> mag = {
            { nodes,     1 },
            { nodes + 1, 2 },
            { nodes + 2, 3 }
        };

        mag.reserve(8);
        //Change each y-coordinate from 0 to the magnitude
        for(auto iter = mag.begin(); iter != mag.end(); ++iter){
            auto cur = iter->first; // pointer to Node
            cur->y = mag[cur]; // could also have used  cur->y = iter->second;
        }

        //Update and print the magnitude of each node
        for(auto iter = mag.begin(); iter != mag.end(); ++iter){
            auto cur = iter->first;
            mag[cur] = std::hypot(cur->x, cur->y);
            std::cout << "The magnitude of (" << cur->x << ", " << cur->y << ") is ";
            std::cout << iter->second << '\n';
        }

        //Repeat the above with the range-based for loop
        for(auto i : mag) {
            auto cur = i.first;
            cur->y = i.second;
            mag[cur] = std::hypot(cur->x, cur->y);
            std::cout << "The magnitude of (" << cur->x << ", " << cur->y << ") is ";
            std::cout << mag[cur] << '\n';
            //Note that in contrast to std::cout << iter->second << '\n'; above,
            // std::cout << i.second << '\n'; will NOT print the updated magnitude
        }
    }

    //swap.ref
    {
        ehmap<int, int> numbers;
        std::cout << std::boolalpha;
        std::cout << "Initially, numbers.empty(): " << numbers.empty() << '\n';

        numbers.emplace(42, 13);
        numbers.insert(std::make_pair(13317, 123));
        std::cout << "After adding elements, numbers.empty(): " << numbers.empty() << '\n';

        ehmap<std::string, std::string>
        m1 { {"γ", "gamma"}, {"β", "beta"}, {"α", "alpha"}, {"γ", "gamma"}, },
        m2 { {"ε", "epsilon"}, {"δ", "delta"}, {"ε", "epsilon"} };

        const auto& ref = *(m1.begin());
        const auto iter = std::next(m1.cbegin());

        std::cout << "──────── before swap ────────\n"
            << "m1: " << m1 << "m2: " << m2 << "ref: {" << ref.first << "," << ref.second << "}"
            << "\niter: " << "{" << iter->first << ", " << iter->second << "}" << '\n';

        m1.swap(m2);

        //crash on gcc ?
#ifndef __GNUC__
        std::cout << "──────── after swap ────────\n"
            << "m1: " << m1 << "m2: " << m2 << "ref: {" << ref.first << "," << ref.second << "}"
            << "\niter: " << "{" << iter->first << ", " << iter->second << "}" << '\n';
#endif
    }

    //merge
    {
        ehmap<std::string, int>
            p{ {"C", 3}, {"B", 2}, {"A", 1}, {"A", 0} },
            q{ {"E", 6}, {"E", 7}, {"D", 5}, {"A", 4} };

        std::cout << "p: " << p << "q: " << q;
        p.merge(q);
        std::cout << "p.merge(q);\n" << "p: " << p << "q: " << q << std::endl;
    }

#if 1
    {
        ehmap<int, char> data {{1, 'a'},{2, 'b'},{3, 'c'},{4, 'd'},
            {5, 'e'},{4, 'f'},{5, 'g'},{5, 'g'}};
        std::cout << "Original:\n" << data << '\n';

        const auto count = data.erase_if([](const auto& item) {
//            auto const [key, val] = item; return (key & 1) == 1;
            return (item.first & 1) == 1;
        });
        std::cout << "Erase items with odd keys:\n" << data << '\n'
            << count << " items removed.\n";
    }

    //hint
    {
       ehmap5<int, char> data {{1, 'a'},{2, 'b'},{3, 'c'},{4, 'd'}, {5, 'e'},{4, 'f'},{5, 'g'},{5, 'g'}};
       std::cout << "Original:\n" << data << '\n';

       auto it = data.find(1);
       it = data.emplace_hint(it, 1, 'c');
       assert(it->second != 'c');

       data.insert_or_assign(it, 1, 'c');
       assert(it->second == 'c');

      data.emplace_hint(data.end(), 1, 'd');
    }

    {
        emhash7::HashMap<uint64_t, uint32_t> emi;
        emi.reserve(10);
        int key = rand();
        emi.insert({key, 0}); emi.insert({key, 1});
        auto iter = emi.find(key);
        assert(iter != emi.end() && iter->second == 0);
#ifdef EMH_ITER_SAFE
        auto iter_next = iter; iter_next++;
        assert(iter_next == emi.end());
#endif
    }

#endif
}

#define TO_KEY(i)  i
static int RandTest(size_t n, int max_loops = 1234567)
{
    printf("n = %d, loop = %d\n", (int)n, (int)max_loops);
    printf("============================== %s ============================\n", __FUNCTION__);
    using keyType = uint64_t;

#if X860
    emilib2::HashMap <keyType, int> shash;
#else
    ehmap7<keyType, int> shash;
#endif

    ehmap5<keyType, int> ehash5;

#if EMH6
    ehmap6<keyType, int> unhash;
#else
    ehmap8<keyType, int> unhash;
#endif

    Sfc4 srng(n + time(0));
    const auto step = n % 2 + 1;
    for (int i = 1; i < n * step; i += step) {
        auto ki = TO_KEY(i);
        ehash5[ki] = unhash[ki] = shash[ki] = (int)srng();
    }

    {
        assert(ehash5 == shash);
        assert(ehash5 == unhash);
    }

    int loops = max_loops;
    while (loops -- > 0) {
        assert(shash.size() == unhash.size()); assert(ehash5.size() == unhash.size());

        const uint32_t type = srng() % 100;
        auto rid  = srng();// n ++;
        auto id   = TO_KEY(rid);
        if (type <= 40 || shash.size() < 1000) {
            shash[id] += type; ehash5[id] += type; unhash[id] += type;

            assert(shash[id] == unhash[id]); assert(ehash5[id] == unhash[id]);
        }
        else if (type < 60) {
            if (srng() % 3 == 0)
                id = unhash.begin()->first;
            else if (srng() % 2 == 0)
                id = shash.begin()->first;
            else
                id = ehash5.begin()->first;

            ehash5.erase(id);
            unhash.erase(unhash.find(id));
            shash.erase(id);

            assert(ehash5.count(id) == unhash.count(id));
            assert(shash.count(id) == unhash.count(id));
        }
        else if (type < 80) {
            auto it = ehash5.begin();
            for (int i = n % 64; i > 0; i--)
                it ++;
            id = it->first;
            unhash.erase(id);
            shash.erase(shash.find(id));
            ehash5.erase(it);
            assert(shash.count(id) == 0);
            assert(ehash5.count(id) == unhash.count(id));
        }
        else if (type < 100) {
            if (unhash.count(id) == 0) {
                const auto vid = (int)rid;
                ehash5.emplace(id, vid);
                assert(ehash5.count(id) == 1);

                assert(shash.count(id) == 0);
                //if (id == 1043)
                shash[id] = vid;
                assert(shash.count(id) == 1);

                assert(unhash.count(id) == 0);
                unhash[id] = shash[id];
                assert(unhash[id] == shash[id]);
                assert(unhash[id] == ehash5[id]);
            } else {
                unhash[id] = shash[id] = 1;
                ehash5.insert_or_assign(id, 1);
                unhash.erase(id);
                shash.erase(id);
                ehash5.erase(id);
            }
        }
        if (loops % 100000 == 0) {
            printf("%d %d\r", loops, (int)shash.size());
#if 1
            assert(ehash5 == shash);
            assert(ehash5 == unhash);
#endif
        }
    }

    printf("\n");
    return 0;
}

static void benchIntRand(int loops = 100000009)
{
    printf("============================== %s ============================\n", __FUNCTION__);
    printf("%s loops = %d\n",__FUNCTION__, loops);
    long sum = 0;
    auto ts = getus();

    auto rseed = randomseed();
    {
        ts = getus();
        Sfc4 srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Sfc4       = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

    {
        ts = getus();
        RomuDuoJr srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("RomuDuoJr  = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
    {
        ts = getus();
        Orbit srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Orbit      = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

#if __SIZEOF_INT128__
    {
        ts = getus();
        Lehmer64 srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("Lehmer64   = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
#endif

    {
        ts = getus();
        std::mt19937_64 srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("mt19937_64 = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }

#if WYHASH_LITTLE_ENDIAN
    {
        ts = getus();
        WyRand srng(rseed);
        for (int i = 1; i < loops; i++)
            sum += srng();
        printf("wyrand     = %4d ms [%ld]\n", (int)(getus() - ts) / 1000, sum);
    }
#endif
}

static void buildRandString(int size, std::vector<std::string>& rndstring, int str_min, int str_max)
{
    std::mt19937_64 srng(randomseed());
    for (int i = 0; i < size; i++)
        rndstring.emplace_back(get_random_alphanum_string(srng() % (str_max - str_min + 1) + str_min));
}

static void benchStringHash(int size, int str_min, int str_max)
{
    printf("============================== %s ============================\n", __FUNCTION__);
    printf("\n%s loops = %d\n", __FUNCTION__, size);
    std::vector<std::string> rndstring;
    rndstring.reserve(size * 4);

    long sum = 0;
    for (int i = 1; i <= 4; i++)
    {
        rndstring.clear();
        buildRandString(size * i, rndstring, str_min + i, str_max * i);
        int64_t start = 0;
        int t_find = 0;

        start = getus();
        for (auto& v : rndstring)
            sum += std::hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("std hash    = %4d ms\n", t_find);

#ifdef WYHASH_LITTLE_ENDIAN
        start = getus();
        for (auto& v : rndstring)
            sum += wyhash(v.data(), v.size(), 1);
        t_find = (getus() - start) / 1000;
        printf("wyhash      = %4d ms\n", t_find);
#endif

#if KOMI_HESH
        start = getus();
        for (auto& v : rndstring)
            sum += komihash(v.data(), v.size(), 1);
        t_find = (getus() - start) / 1000;
        printf("komi_hash   = %4d ms\n", t_find);
#endif

#ifdef AHASH_AHASH_H
        start = getus();
        for (auto& v : rndstring)
            sum += ahash64(v.data(), v.size(), 1);
        t_find = (getus() - start) / 1000;
        printf("ahash       = %4d ms\n", t_find);
#endif

        start = getus();
        for (auto& v : rndstring)
            sum += robin_hood::hash_bytes(v.data(), v.size());
        t_find = (getus() - start) / 1000;
        printf("martius hash= %4d ms\n", t_find);

#if ABSL_HASH && ABSL
        start = getus();
        for (auto& v : rndstring)
            sum += absl::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("absl hash   = %4d ms\n", t_find);
#endif

#ifdef PHMAP_VERSION_MAJOR
        start = getus();
        for (auto& v : rndstring)
            sum += phmap::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000;
        printf("phmap hash  = %4d ms\n", t_find);
#endif
        putchar('\n');
    }
    printf("sum = %ld\n", sum);
}

int main(int argc, char* argv[])
{
    TestApi();
    benchIntRand(1e8+8);
    benchStringHash(1e6+6, 6, 16);

    size_t n = (int)1e6, loop = 12345678;
    if (argc > 1 && isdigit(argv[1][0]))
        n = atoi(argv[1]);
    if (argc > 2 && isdigit(argv[2][0]))
        loop = atoi(argv[2]);

    RandTest(n, loop);

    return 0;
}

