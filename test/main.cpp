#ifndef _WIN32
//#define BOOST_TEST_MODULE hash_map_tests
//#include <boost/test/unit_test.hpp>
//#include "utils.h"
#endif

//#define TKey 2
//#define EMH_SAFE_PSL 1
//#define EMH_STATIS   1234567
//#define EMH_PSL_LINEAR 1

//#define EMH_PSL      32
//#define EMH_QUADRATIC 1

//#define EMH_IDENTITY_HASH 1
#include "eutil.h"
//#define EMH_WYHASH_HASH 1
//#define EMH_ITER_SAFE 1
//#define EMH_FIND_HIT  1
//#define EMH_BUCKET_INDEX 2

#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"

#include "emilib/emilib2o.hpp"
#include "emilib/emilib2s.hpp"
#include "emilib/emilib2ss.hpp"


//#include "martin/robin_hood.h"
#include "martin/unordered_dense.h"
#include "phmap/phmap.h"

#if CXX20
#include <string_view>
struct string_hash
{
    using is_transparent = void;
#if WYHASH_LITTLE_ENDIAN
    std::size_t operator()(const char* key)             const { return wyhash(key, strlen(key), 0); }
    std::size_t operator()(const std::string& key)      const { return wyhash(key.data(), key.size(), 0); }
    std::size_t operator()(const std::string_view& key) const { return wyhash(key.data(), key.size(), 0); }
#else
    std::size_t operator()(const char* key)             const { return std::hash<std::string_view>()(std::string_view(key, strlen(key))); }
    std::size_t operator()(const std::string& key)      const { return std::hash<std::string_view>()(key); }
    std::size_t operator()(const std::string_view& key) const { return std::hash<std::string_view>()(key); }
#endif
};

struct string_equal
{
    using is_transparent = int;


    bool operator()(const std::string_view& lhs, const std::string& rhs) const {
        return lhs == rhs;
    }

    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return lhs == rhs;
    }

    bool operator()(const char* lhs, const std::string& rhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }
    bool operator()(const std::string& rhs, const char* lhs) const {
        return std::strcmp(lhs, rhs.data()) == 0;
    }
};

static int find_strview_test()
{
    {
        emhash7::HashMap<std::string, int, string_hash, string_equal> map;
        std::string_view vkey = "key";
        std::string      skey = "key";
        assert(vkey == skey);

        map.emplace(vkey, 0);
        map.emplace(skey, 1);
        const auto it = map.find(vkey); // fail
        assert(it == map.find("vkey"));
        assert(it == map.find(skey));
    }

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
    Foo() = default;
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
#define ehmap  emilib3::HashMap
#else
#define ehmap  emhash8::HashMap
#endif
#define ehmap5 emhash5::HashMap
#define ehmap6 emhash6::HashMap
#define ehmap7 emhash7::HashMap
#define ehmap8 emhash8::HashMap

//TODO template
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
        assert(m2.at(2) == "second");
        m2.insert({3, "null"}).first->second = "third";
        m2.emplace(4, "null").first->second = "four";
        m2.insert(std::pair<int, std::string>(5, "insert"));
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
        assert(m2.empty());

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

    //copy
    {
        ehmap<short, int> dict = {{1, 1}, {2, 2}, {3, 3}};
        dict.reserve(1u << 20);// overview
#if EMH_SAVE_MEM
        assert(dict.bucket_count() <= (2 << 16));
#endif
        dict.shrink_to_fit();
        assert(dict.bucket_count() <= 32);

        dict.reserve(1024);
        for (short i = 0; i < 1024; i++) {
            dict[i] = 0;
            decltype(dict) dict2 = dict;
            assert(dict2 == dict);
        }
        assert(dict.size() == 1024);

        for (int i = 0; i < 1024; i++) {
            dict.erase((short)i);
            decltype(dict) dict3 = {{1, 1}, {2, 2}, {3, 3}};
            dict3 = dict;
            assert(dict3 == dict);
        }
        assert(dict.size() == 0);
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
        m.emplace(std::make_pair("b", "b"));
        m.emplace(std::make_pair("b", "abcd"));
        // uses pair's template constructor
        m.emplace("d", "ddd");
        assert(m.size() == 3);
        assert(m["d"] == "ddd");
        assert(m["b"] == "b");

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
            if(it->first % 2 != 0) {
                if constexpr (std::is_void_v<decltype(c.erase(it))>) {
                    c.erase(it++);
                } else {
//                    c.erase(it++);
                    it = c.erase(it);
                }
            }
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
        for(auto& i : mag) {
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
        for(auto& i : mag) {
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
#if 0
        std::cout << "──────── after swap ────────\n"
            << "m1: " << m1 << "m2: " << m2 << "ref: {" << ref.second << "}"
            << "\niter: " << "{" << iter->second << "}" << '\n';
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

#if 1
    {
        ehmap<uint64_t, uint32_t> emi;
        emi.reserve(1000);
        uint64_t key = rand();
        emi.insert({key, 0}); emi.emplace(key, 1);
        auto it = emi.try_emplace(key, 0); assert(!it.second);
        it = emi.try_emplace(key + 1, 1); assert(it.second);

        emi.shrink_to_fit();
        auto iter = emi.find(key);
        assert(iter != emi.end());
        assert(iter->second == 0);
#ifdef EMH_ITER_SAFE
        auto iter_next = iter; iter_next++;
        iter_next++;
        assert(iter_next == emi.end());
#endif
    }
#endif

#if CXX20
    {
        ehmap<std::string, int, string_hash, string_equal> map;
        std::string_view vkey = "key";
        std::string      skey = "key";

        auto its = map.emplace(skey, 1);
        auto itv = map.emplace(vkey, 0);
        assert(!itv.second);
        const auto it = map.find(vkey); // fail
        assert(it == map.find(skey));
        assert(it == map.find("key"));
    }
#endif

#endif
}

#if FIB_HASH
    #define BintHasher Int64Hasher<keyType>
#elif HOOD_HASH
    #define BintHasher robin_hood::hash<keyType>
#elif STD_HASH
    #define BintHasher std::hash<keyType>
#else
    #define BintHasher ankerl::unordered_dense::hash<keyType>
#endif

#define TO_KEY(i)  i
static int RandTest(size_t n, int max_loops = 1234567)
{
    printf("n = %d, loop = %d\n", (int)n, (int)max_loops);
    printf("============================== %s ============================\n", __FUNCTION__);
    using keyType = uint64_t;

#if EMI == 0
    emilib2::HashMap <keyType, int, BintHasher> ehash;
#elif EMI == 3
    emilib3::HashMap <keyType, int, BintHasher> ehash;
#elif EMI == 1
    emilib::HashMap <keyType, int, BintHasher> ehash;
#else
    ankerl::unordered_dense::map<keyType, int> ehash;
#endif

    ehmap8<keyType, int, BintHasher> ehash8;

#if EMH == 5
    ehmap5<keyType, int, BintHasher> unhash;
#elif EMH == 6
    ehmap6<keyType, int, BintHasher> unhash;
#else
    //emilib3::HashMap <keyType, int, BintHasher> ehash;
    ehmap7<keyType, int, BintHasher> unhash;
#endif

    WyRand srng(time(0));
    const auto step = n % 2 + 1;
    for (size_t i = 1; i < n * step; i += step) {
        auto ki = TO_KEY(i);
        ehash8[ki] = unhash[ki] = ehash[ki] = (int)srng();
    }

    {
        assert(ehash8 == ehash);
        assert(ehash8 == unhash);
    }

    auto nows = time(0);
    int loops = max_loops;
    while (loops -- > 0) {
        assert(ehash.size() == unhash.size());
        assert(ehash8.size() == unhash.size());

        const uint32_t type = int(srng() % 100);
        auto rid  = srng();// n ++;
        auto id   = TO_KEY(rid);
        if (type <= 40 || ehash8.size() < 1000) {
          auto cnid = ehash8.count(id);
          assert(cnid == unhash.count(id));

            if (type % 3 == 0) {
                ehash[id] += type; ehash8[id] += type; unhash[id] += type;
            } else if (type % 2 == 0) {
                ehash.insert_or_assign(id, type + 2);
                ehash8.insert_or_assign(id, type + 2);
                unhash.insert_or_assign(id, type + 2);
            } else {
                ehash.emplace(id, type + 1);
                ehash8.emplace(id, type + 1);
                unhash.emplace(id, type + 1);
            }

            assert(ehash8[id] == ehash[id]);
            if (ehash[id] != unhash[id]) {
                cnid = unhash.count(id);
                unhash.emplace(id, type + 1);
                printf("%d e=%d %d %d %d\n", type, cnid, ehash[id], ehash8[id], unhash.at(id));
            }
        }
        else if (type < 60) {
            if (srng() % 3 == 0)
                id = unhash.begin()->first;
            else if (srng() % 2 == 0)
                id = ehash.begin()->first;
            else
                id = ehash8.last()->first;

            ehash8.erase(id);
            ehash.erase(id);
            unhash.erase(unhash.find(id));

            assert(ehash.count(id) == unhash.count(id));
            assert(ehash8.count(id) == unhash.count(id));
        }
        else if (type < 80) {
            auto it = ehash8.begin();
            for (int i = n % 64; i > 0; i--)
                it ++;
            id = it->first;
            unhash.erase(id);
            ehash.erase(ehash.find(id));
            ehash8.erase(it);
            assert(ehash.count(id) == 0);
            assert(ehash8.count(id) == unhash.count(id));
        }
        else if (type < 100) {
            if (ehash8.count(id) == 0) {
                const auto vid = (int)rid;
                ehash8.emplace(id, vid);
                assert(ehash8.count(id) == 1);

                assert(ehash.count(id) == 0);
                //if (id == 1043)
                ehash[id] = vid;
                assert(ehash.count(id) == 1);

                assert(unhash.count(id) == 0);
                unhash[id] = ehash[id];
                assert(ehash8[id] == ehash[id]);
                assert(unhash[id] == ehash8[id]);
            }
            else {
                unhash[id] = ehash[id] = 1;
                ehash8.insert_or_assign(id, 1);
                unhash.erase(id);
                ehash.erase(id);
                ehash8.erase(id);
            }
        }
        if (loops % 1000'000 == 0) {
            printf("loops = %d %d\n", loops, (int)ehash.size());
            assert(ehash8.operator==(ehash));
            assert(ehash8 == unhash);
        }
    }

    printf("time use %d sec\n", int(time(0) - nows));
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
    const uint64_t rseed = rand();

    long sum = 0;
    for (int i = 1; i <= 6; i++)
    {
        rndstring.clear();
        printf("%d - %d bytes\n", str_min * i, str_max * i);
        buildRandString(size * i, rndstring, str_min * i, str_max * i);

        int64_t start = 0;
        int64_t t_find = 0;

        start = getus();
        for (const auto& v : rndstring)
            sum = std::hash<std::string>()(v);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("std hash    = %4d ms\n", (int)t_find);

#ifdef WYHASH_LITTLE_ENDIAN
        start = getus();
        for (const auto& v : rndstring)
            sum += wyhash(v.data(), v.size(), rseed);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("wyhash      = %4d ms\n", (int)t_find);
#endif

#if KOMI_HASH
        start = getus();
        for (const auto& v : rndstring)
            sum += komihash(v.data(), v.size(), rseed);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("komi_hash   = %4d ms\n", (int)t_find);
#endif

#if RAPID_HASH
        start = getus();
        for (const auto& v : rndstring)
            sum += rapidhashMicro_withSeed(v.data(), v.size(), rseed);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("rapid_hash  = %4d ms\n", (int)t_find);
#endif

#if A5_HASH
        start = getus();
        for (const auto& v : rndstring)
            sum += a5hash(v.data(), v.size(), rseed);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("a5_hash     = %4d ms\n", (int)t_find);
#endif

#ifdef AHASH_AHASH_H
        start = getus();
        for (const auto& v : rndstring)
            sum += ahash64(v.data(), v.size(), rseed);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("ahash       = %4d ms\n", (int)t_find);

        start = getus();
        for (const auto& v : rndstring) {
            ahash::Hasher hasher{rseed};
            hasher.consume(v.data(), v.size());
            sum += hasher.finalize();
        }
        t_find = (getus() - start) / 1000; assert(sum);
        printf("acxxhash    = %4d ms\n", (int)t_find);
#endif

#if ROBIN_HOOD_VERSION_MAJOR
        start = getus();
        for (const auto& v : rndstring)
            sum += robin_hood::hash_bytes(v.data(), v.size());
        t_find = (getus() - start) / 1000; assert(sum);
        printf("martius hash= %4d ms\n", (int)t_find);
#endif

#if 0
        start = getus(); sum += 0;
        for (const auto& v : rndstring)
            sum += emhash8::HashMap<int,int>::wyhashstr (v.data(), v.size());
        t_find = (getus() - start) / 1000; assert(sum);
        printf("emhash8 hash= %4d ms\n", (int)t_find);
#endif

        start = getus(); sum += 0;
        for (const auto& v : rndstring)
          sum += ankerl::unordered_dense::detail::wyhash::hash(v.data(), v.size());
        t_find = (getus() - start) / 1000; assert(sum);
        printf("ankerl hash = %4d ms\n", (int)t_find);

#ifdef ABSL_HASH
        start = getus();
        for (const auto& v : rndstring)
            sum += absl::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("absl hash   = %4d ms\n", (int)t_find);
#endif

#ifdef PHMAP_VERSION_MAJOR
        start = getus();
        for (const auto& v : rndstring)
            sum += phmap::Hash<std::string>()(v);
        t_find = (getus() - start) / 1000; assert(sum);
        printf("phmap hash  = %4d ms\n", (int)t_find);
#endif
        putchar('\n');
    }
    printf(" sum += %ld\n", sum);
}

template<typename MAP>
static void TestHighLoadFactor(int id)
{
    std::random_device rd;
    const auto rand_key = rd() + getus();
#if 1
    WyRand srngi(rand_key), srnge(rand_key);
#else
    std::mt19937_64 srngi(rand_key), srnge(rand_key);
#endif

    const auto max_lf   = 0.999; //<= 0.9999f
    const auto vsize    = 1u << (20 + id % 6);//must be power of 2
    MAP myhash(vsize, (float)max_lf);
    //emhash7::HashMap<int64_t, int> myhash(vsize, max_lf);
    //emhash5::HashMap<int64_t, int> myhash(vsize, max_lf);
    //ankerl::unordered_dense::map<int64_t, int> myhash(vsize / 2); myhash.max_load_factor(max_lf);

    auto nowus = getus();
    for (size_t i = 0; i < size_t(vsize * max_lf); i++)
        myhash.emplace(srngi(), 0);
    //assert(myhash.bucket_count() == vsize); //no rehash

    //while (myhash.load_factor() < max_lf - 1e-3) myhash.emplace(srngi(), 0);
    const auto insert_time = getus() - nowus; nowus = getus();
    //erase & insert at a fixed load factor
    for (size_t i = 0; i < vsize; i++) {
        myhash.erase(srnge()); //erase a exist old key
        myhash[srngi()] = 1;
    }
    const auto erase_time = getus() - nowus;
    printf("vsize = %d, load factor = %.4f, insert/erase = %ld/%ld ms\n",
        vsize, myhash.load_factor(), insert_time / 1000, erase_time / 1000);
    //assert(myhash.bucket_count() == vsize); //no rehash
    //assert(myhash.load_factor() >= max_lf - 0.001);
}

int main(int argc, char* argv[])
{
    printInfo(nullptr);
    srand((unsigned)time(nullptr));

    if (argc == 2) {
        TestApi();
        benchIntRand(123456789);
        benchStringHash(1234567, 8, 32);
    }

    size_t n = (int)1e7, loop = 12345678;
    if (argc > 1 && isdigit(argv[1][0]))
        n = atoi(argv[1]);
    if (argc > 2 && isdigit(argv[2][0]))
        loop = atoi(argv[2]);

#ifndef GCOV
    RandTest(n, (int)loop);
#endif

    for (int i = 0; i < 6; i++) {
        TestHighLoadFactor<emhash7::HashMap<uint64_t, int>>(i);
        TestHighLoadFactor<emhash8::HashMap<uint64_t, int>>(i);
    }

    return 0;
}
