#include <cstdio>
#include <string>
#include "emhash/hash_set8.hpp"
int main() {
    printf("Test 1: basic insert/erase/copy/clear\n");
    {
        emhash8::HashSet<std::string> s;
        s.insert("apple");
        s.insert("banana");
        s.insert("cherry");
        s.insert("date");
        s.insert("elderberry");
        printf("size=%zu\n", s.size());
        s.erase("cherry");
        printf("after erase size=%zu\n", s.size());
        {
            emhash8::HashSet<std::string> s2(s);
            printf("copy size=%zu\n", s2.size());
        }
        printf("after s2 dtor\n");
        s.clear();
        printf("after clear size=%zu\n", s.size());
    }
    printf("All done\n");
    return 0;
}
