#include <iostream>
#include <string>
#include "hash_set8.hpp"

int main() {
    emhash8::HashSet<std::string> set;

    // Insert
    set.insert("apple");
    set.insert("banana");
    set.insert("cherry");
    set.insert("apple");  // duplicate, ignored

    // Lookup
    std::cout << "contains 'apple': " << (set.count("apple") ? "yes" : "no") << "\n";
    std::cout << "contains 'grape': " << (set.count("grape") ? "yes" : "no") << "\n";

    // Iterate
    std::cout << "set contents:\n";
    for (auto& v : set) {
        std::cout << "  " << v << "\n";
    }

    // Erase
    set.erase("banana");
    std::cout << "size after erase: " << set.size() << "\n";

    return 0;
}
