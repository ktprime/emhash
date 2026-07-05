#include <string>
#include <cstdio>
#include <functional>
#include "emilib/emihset2.hpp"

// Hack to access private members for debugging
namespace emilib2 {
template <typename K> struct DebugHashSet : HashSet<K> {
    uint8_t* get_states() { return this->_states; }
    std::string* get_keys() { return this->_keys; }
    size_t get_num_buckets() { return this->_num_buckets; }
};
} // namespace emilib2

int main() {
    printf("=== Bucket 22 Analysis ===\n");

    emilib2::DebugHashSet<std::string> s1;
    for (int i = 0; i < 55; i++) {
        s1.insert("key_" + std::to_string(i));
    }

    printf("After inserting 55 keys:\n");
    printf("  bucket_count=%zu\n", s1.get_num_buckets());

    // Check bucket 22 state and key
    auto* states = s1.get_states();
    auto* keys = s1.get_keys();

    printf("  _states[22] = %02x\n", states[22]);
    printf("  _keys[22] = '%s'\n", keys[22].c_str());

    // Now try to insert key_55
    auto result = s1.insert("key_55");
    printf("Insert key_55: success=%d, bucket=%zu\n", result.second, result.first.bucket());

    printf("After insert attempt:\n");
    printf("  _states[22] = %02x\n", states[22]);
    printf("  _keys[22] = '%s'\n", keys[22].c_str());

    return 0;
}
