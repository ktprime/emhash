#include <iostream>
#include <string>
#include "emhash/hash_table8.hpp"

int main() {
    // Basic map usage - drop-in replacement for std::unordered_map
    emhash8::HashMap<int, std::string> map;

    // Insert
    map[1] = "hello";
    map[2] = "world";
    map.insert({3, "emhash"});

    // Lookup
    std::cout << "map[1] = " << map[1] << "\n";
    std::cout << "map[2] = " << map[2] << "\n";

    // Find
    auto it = map.find(3);
    if (it != map.end()) {
        std::cout << "found: " << it->second << "\n";
    }

    // Iterate
    std::cout << "all entries:\n";
    for (auto& [k, v] : map) {
        std::cout << "  " << k << " -> " << v << "\n";
    }

    // Size and erase
    std::cout << "size: " << map.size() << "\n";
    map.erase(2);
    std::cout << "after erase: " << map.size() << "\n";

    return 0;
}
