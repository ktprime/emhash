/**
 * MIT License
 *
 * Copyright (c) 2017 Thibaut Goetghebuer-Planchon <tessil@gmx.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if 0
#define BOOST_TEST_MODULE hash_map_tests
#include <boost/test/unit_test.hpp>
#endif

#include "eutil.h"
#include "../hash_table5.hpp"
#include "../hash_table6.hpp"
#include "../hash_table7.hpp"
#include "../hash_table8.hpp"
#include "../thirdparty/emilib/emilib2.hpp"

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

template <class Os, class U, class V>
Os& operator<<(Os& os, const std::pair<U, V>& p) {
    return os << p.first << ":" << p.second;
}

// print out a container
template <class Os, class Co>
Os& operator<<(Os& os, const Co& co) {
    os << "{";
    for (auto const& i : co) { os << ' ' << i; }
    return os << " }\n";
}

static void unit_test()
{
    {
        // default constructor: empty map
        emhash5::HashMap<std::string, std::string> m1;
        // list constructor
        emhash5::HashMap<int, std::string> m2 =
        {
            {1, "foo"},
            {3, "bar"},
            {2, "baz"},
        };

        // copy constructor
        emhash5::HashMap<int, std::string> m3 = m2;

        // move constructor
        emhash5::HashMap<int, std::string> m4 = std::move(m2);

        // range constructor
        std::vector<std::pair<std::bitset<8>, int>> v = { {0x12, 1}, {0x01,-1} };
        emhash5::HashMap<std::bitset<8>, double> m5(v.begin(), v.end());

        //Option 1 for a constructor with a custom Key type
        // Define the KeyHash and KeyEqual structs and use them in the template
        emhash5::HashMap<Key, std::string, KeyHash, KeyEqual> m6 = {
            { {"John", "Doe"}, "example"},
            { {"Mary", "Sue"}, "another"}
        };

        //Option 2 for a constructor with a custom Key type
        // Define a const == operator for the class/struct and specialize std::hash
        // structure in the std namespace
        emhash5::HashMap<Foo, std::string> m7 = {
            { Foo(1), "One"}, { 2, "Two"}, { 3, "Three"}
        };

#if CXX20
        struct Goo {int val; };
        auto hash = [](const Goo &g){ return std::hash<int>{}(g.val); };
        auto comp = [](const Goo &l, const Goo &r){ return l.val == r.val; };
        emhash5::HashMap<Goo, double, decltype(hash), decltype(comp)> m8;
#endif

        emhash5::HashMap<int,char> example = {{1,'a'},{2,'b'}};
        for(int x: {2, 5}) {
            if(example.contains(x)) {
                std::cout << x << ": Found\n";
            } else {
                std::cout << x << ": Not found\n";
            }
        }
    }

    {
        emhash5::HashMap<int, std::string> dict = {{1, "one"}, {2, "two"}};
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
        emhash5::HashMap<int, std::string> dict2;
        dict2.insert(dict.begin(), dict.end());
        for(auto& p: dict2)
            std::cout << " " << p.first << " => " << p.second << '\n';
    }

    {
        emhash6::HashMap<std::string, std::string> m;
        // uses pair's move constructor
        m.emplace(std::make_pair(std::string("a"), std::string("a")));
        // uses pair's converting move constructor
        m.emplace(std::make_pair("b", "abcd"));
        m.emplace(std::move(std::make_pair("b", "abcd")));
        // uses pair's template constructor
        m.emplace("d", "ddd");
        /*
        // uses pair's piecewise constructor
        m.emplace(std::piecewise_construct,
        std::forward_as_tuple("c"),
        std::forward_as_tuple(10, 'c'));
        // as of C++17, m.try_emplace("c", 10, 'c'); can be used
        */

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

        emhash5::HashMap<std::string, std::string> myMap;
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

        emhash5::HashMap<char, int> letter_counts {{'a', 27}, {'b', 3}, {'c', 1}};

        print("letter_counts initially contains: ", letter_counts);

        letter_counts['b'] = 42;  // updates an existing value
        letter_counts['x'] = 9;  // inserts a new value

        print("after modifications it contains: ", letter_counts);

        // count the number of occurrences of each word
        // (the first call to operator[] initialized the counter with zero)
        emhash5::HashMap<std::string, int>  word_map;
        for (const auto &w : { "this", "sentence", "is", "not", "a", "sentence",
                "this", "sentence", "is", "a", "hoax"}) {
            ++word_map[w];
        }
        word_map["that"]; // just inserts the pair {"that", 0}

        //for (const auto [word, id, count] : word_map) { std::cout << count << " occurrences of word '" << word << "'\n"; }
        for (const auto &it : word_map) { std::cout << it.second << " occurrences of word '" << it.first << "'\n"; }
    }

    {
        emhash7::HashMap<int, std::string, std::hash<int>> c = {
            {1, "one" }, {2, "two" }, {3, "three"},
            {4, "four"}, {5, "five"}, {6, "six"  }
        };

        // erase all odd numbers from c
        for(auto it = c.begin(); it != c.end(); ) {
            printf("%d:%d:%s\n", it->first, (int)it.bucket(), it->second.data());
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
        emhash8::HashMap<int, char> container{{1, 'x'}, {2, 'y'}, {3, 'z'}};
        auto print = [](std::pair<int, char>& n) {
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

    struct Node { double x, y; };

    {
        Node nodes[3] = { {1, 0}, {2, 0}, {3, 0} };

        //mag is a map mapping the address of a Node to its magnitude in the plane
        emhash5::HashMap<Node*, double> mag = {
            { nodes,     1 },
            { nodes + 1, 2 },
            { nodes + 2, 3 }
        };

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

    {
        Node nodes[3] = { {1, 0}, {2, 0}, {3, 0} };

        //mag is a map mapping the address of a Node to its magnitude in the plane
        emhash5::HashMap<Node *, double> mag = {
            { nodes,     1 },
            { nodes + 1, 2 },
            { nodes + 2, 3 }
        };

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

    {
        emhash5::HashMap<int, int> numbers;
        //        std::cout << std::boolalpha;
        std::cout << "Initially, numbers.empty(): " << numbers.empty() << '\n';

        numbers.emplace(42, 13);
        numbers.insert(std::make_pair(13317, 123));
        std::cout << "After adding elements, numbers.empty(): " << numbers.empty() << '\n';


        std::unordered_map<std::string, std::string>
            m1 { {"γ", "gamma"}, {"β", "beta"}, {"α", "alpha"}, {"γ", "gamma"}, },
               m2 { {"ε", "epsilon"}, {"δ", "delta"}, {"ε", "epsilon"} };

        const auto& ref = *(m1.begin());
        const auto iter = std::next(m1.cbegin());

        std::cout << "──────── before swap ────────\n"
            << "m1: " << m1 << "m2: " << m2 << "ref: " << ref
            << "\niter: " << *iter << '\n';

        m1.swap(m2);

        std::cout << "──────── after swap ────────\n"
            << "m1: " << m1 << "m2: " << m2 << "ref: " << ref
            << "\niter: " << *iter << '\n';
    }
}

#define TO_KEY(i)  i
static int TestHashMap(int n, int max_loops = 1234567)
{
	using keyType = long;

#if X86
#ifndef KEY_CLA

    emhash5::HashMap <keyType, int> ehash5;
#if X86
    emilib2::HashMap <keyType, int> ehash2;
#else
    emhash7::HashMap <keyType, int> ehash2;
#endif

    emhash8::HashMap <keyType, int> unhash;

    Sfc4 srng(n);
    const auto step = n % 2 + 1;
    for (int i = 1; i < n * step; i += step) {
        auto ki = TO_KEY(i);
        ehash5[ki] = unhash[ki] = ehash2[ki] = (int)srng();
    }

    int loops = max_loops;
    while (loops -- > 0) {
        assert(ehash2.size() == unhash.size()); assert(ehash5.size() == unhash.size());

        const uint32_t type = srng() % 100;
        auto rid  = n ++;
        auto id   = TO_KEY(rid);
        if (type <= 40 || ehash2.size() < 1000) {
            ehash2[id] += type; ehash5[id] += type; unhash[id] += type;

            assert(ehash2[id] == unhash[id]); assert(ehash5[id] == unhash[id]);
        }
        else if (type < 60) {
            if (srng() % 3 == 0)
                id = unhash.begin()->first;
            else if (srng() % 2 == 0)
                id = ehash2.begin()->first;
            else
                id = ehash5.begin()->first;

            ehash5.erase(id);
            unhash.erase(id);
            ehash2.erase(id);

            assert(ehash5.count(id) == unhash.count(id));
            assert(ehash2.count(id) == unhash.count(id));
        }
        else if (type < 80) {
            auto it = ehash5.begin();
            for (int i = n % 64; i > 0; i--)
                it ++;
            id = it->first;
            unhash.erase(id);
            ehash2.erase(ehash2.find(id));
            ehash5.erase(it);
            assert(ehash2.count(id) == 0);
            assert(ehash5.count(id) == unhash.count(id));
        }
        else if (type < 100) {
            if (unhash.count(id) == 0) {
                const auto vid = (int)rid;
                ehash5.emplace(id, vid);
                assert(ehash5.count(id) == 1);
                assert(ehash2.count(id) == 0);

                //if (id == 1043)
                ehash2.insert(id, vid);
                assert(ehash2.count(id) == 1);

                unhash[id] = ehash2[id];
                assert(unhash[id] == ehash2[id]);
                assert(unhash[id] == ehash5[id]);
            } else {
                unhash[id] = ehash2[id] = ehash5[id] = 1;
                unhash.erase(id);
                ehash2.erase(id);
                ehash5.erase(id);
            }
        }
        if (loops % 100000 == 0) {
            printf("%d %d\r", loops, (int)ehash2.size());
#if 0
            ehash2.shrink_to_fit();
            //ehash5.shrink_to_fit();

            uint64_t sum1 = 0, sum2 = 0, sum3 = 0;
            for (auto v : unhash) sum1 += v.first * v.second;
            for (auto v : ehash2) sum2 += v.first * v.second;
            for (auto v : ehash5) sum3 += v.first * v.second;
            assert(sum1 == sum2);
            assert(sum1 == sum3);
#endif
        }
    }

    printf("\n");
#endif
#endif

    return 0;
}

int main()
{
    unit_test();
	TestHashMap(1e6);
    return 0;
}
