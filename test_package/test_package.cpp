#include <emhash/hash_table5.hpp>
#include <emhash/hash_table6.hpp>
#include <emhash/hash_table7.hpp>
#include <emhash/hash_table8.hpp>
#include <emhash/hash_set2.hpp>
#include <emhash/hash_set4.hpp>
#include <emilib/emihmap1.hpp>
#include <emilib/emihmap2.hpp>
#include <emilib/emihmap3.hpp>
#include <iostream>
#include <string>

template <typename Map>
bool test_int_map() {
    Map map;
    map[1] = 42;
    map[2] = 43;
    return map.size() == 2 && map[1] == 42 && map[2] == 43;
}

template <typename Set>
bool test_int_set() {
    Set set;
    set.insert(1);
    set.insert(2);
    set.insert(1);
    return set.size() == 2 && set.count(1) == 1;
}

int main() {
    bool ok = true;

    ok &= test_int_map<emhash5::HashMap<int, int>>();
    ok &= test_int_map<emhash6::HashMap<int, int>>();
    ok &= test_int_map<emhash7::HashMap<int, int>>();
    ok &= test_int_map<emhash8::HashMap<int, int>>();

    ok &= test_int_set<emhash2::HashSet<int>>();
    ok &= test_int_set<emhash4::HashSet<int>>();

    ok &= test_int_map<emilib1::HashMap<int, int>>();
    ok &= test_int_map<emilib2::HashMap<int, int>>();
    ok &= test_int_map<emilib3::HashMap<int, int>>();

    if (ok) {
        std::cout << "emhash test_package: SUCCESS" << std::endl;
        return 0;
    }
    std::cout << "emhash test_package: FAILED" << std::endl;
    return 1;
}