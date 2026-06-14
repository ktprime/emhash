// Debug emilib2o - print _states array using a derived class hack
#include <cstdio>
#include <cstdint>
#include <cstring>

// Forward declare and access internals via template specialization
namespace emilib2 {
template <typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<KeyT>>
class HashMap;
}

// We need to include the header first
#include "emilib/emilib2o.hpp"

// Now create a debug wrapper
template<typename K, typename V>
class DebugHashMap : public emilib2::HashMap<K, V> {
public:
    using Base = emilib2::HashMap<K, V>;
    using Base::Base;

    void print_states() const {
        // Access through public interface only
        printf("bucket_count = %u\n", Base::bucket_count());
        printf("size = %u\n", Base::size());

        // Iterate and print all elements
        printf("Elements:\n");
        for (auto it = Base::begin(); it != Base::end(); ++it) {
            printf("  key=%d value=%d\n", it->first, it->second);
        }
    }
};

int main() {
    DebugHashMap<int, int> em(4, 0.8f);

    em.emplace(-37009, -36865);
    printf("After emplace(-37009):\n");
    em.print_states();

    em.emplace(1862336511, -37009);
    printf("\nAfter emplace(1862336511):\n");
    em.print_states();

    printf("\nFind results:\n");
    printf("  find(-37009): %s\n", em.find(-37009) != em.end() ? "FOUND" : "NOT FOUND");
    printf("  find(1862336511): %s\n", em.find(1862336511) != em.end() ? "FOUND" : "NOT FOUND");

    return 0;
}
