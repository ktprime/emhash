#include "emilib/emihmap4.hpp"
#include <cstdio>
#include <cstdint>
#include <cassert>
#include <string>

int main() {
    using Map = emilib4::HashMap<int64_t, int64_t>;

    printf("1. Basic CRUD...\n");
    Map m;
    m[1] = 10; m[2] = 20; m[3] = 30;
    assert(m.size() == 3);
    assert(m.contains(1) && m.contains(2) && m.contains(3));
    assert(m[1] == 10 && m[2] == 20 && m[3] == 30);
    m.erase(2);
    assert(!m.contains(2) && m.size() == 2);
    m.clear();
    assert(m.empty());
    printf("   OK\n");

    printf("2. Large sequential (100K)...\n");
    Map m2;
    for (int i = 0; i < 100000; i++) m2[i] = i * 10;
    assert(m2.size() == 100000);
    for (int i = 0; i < 100000; i++) {
        auto it = m2.find(i);
        assert(it != m2.end() && it->second == i * 10);
    }
    for (int i = 0; i < 100000; i++) m2.erase(i);
    assert(m2.empty());
    printf("   OK\n");

    printf("3. String keys...\n");
    emilib4::HashMap<std::string, int> ms;
    ms["hello"] = 1; ms["world"] = 2;
    assert(ms.size() == 2);
    assert(ms["hello"] == 1);
    ms.erase("hello");
    assert(!ms.contains("hello"));
    printf("   OK\n");

    printf("4. Copy/move...\n");
    Map m3;
    for (int i = 0; i < 1000; i++) m3[i] = i;
    Map m4 = m3;
    assert(m4.size() == 1000 && m4[500] == 500);
    Map m5 = std::move(m3);
    assert(m5.size() == 1000 && m5[500] == 500);
    printf("   OK\n");

    printf("5. Random keys (50K)...\n");
    Map m6;
    srand(42);
    for (int i = 0; i < 50000; i++) {
        auto k = static_cast<int64_t>(rand()) * rand();
        m6[k] = k;
    }
    printf("   size=%zu, checking...\n", m6.size());
    srand(42);
    int found = 0;
    for (int i = 0; i < 50000; i++) {
        auto k = static_cast<int64_t>(rand()) * rand();
        if (m6.contains(k)) found++;
    }
    printf("   found=%d size=%zu\n", found, m6.size());
    printf("   OK\n");

    printf("\nAll tests passed!\n");
    return 0;
}
