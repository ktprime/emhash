#include <cstdio>
#include <cassert>
#include <string>
#include "emhash/hash_table5.hpp"
#include "emhash/hash_table6.hpp"
#include "emhash/hash_table7.hpp"
#include "emhash/hash_table8.hpp"

int main() {
    // Test 1: Empty map operations
    {
        emhash5::HashMap<int,int> m;
        assert(m.find(0) == m.end());
        assert(m.size() == 0);
        m.clear();
        assert(m.size() == 0);
    }
    
    // Test 2: Single element
    {
        emhash5::HashMap<int,int> m;
        m[1] = 100;
        assert(m.size() == 1);
        assert(m[1] == 100);
        m.erase(1);
        assert(m.size() == 0);
        assert(m.find(1) == m.end());
    }
    
    // Test 3: Erase all elements one by one
    {
        emhash7::HashMap<int,int> m;
        for (int i = 0; i < 100; i++) m[i] = i;
        for (int i = 0; i < 100; i++) {
            assert(m.erase(i) == 1);
            assert(m.size() == (size_t)(99 - i));
        }
        assert(m.size() == 0);
    }
    
    // Test 4: Copy after erase
    {
        emhash8::HashMap<int,int> m;
        for (int i = 0; i < 50; i++) m[i] = i;
        for (int i = 0; i < 25; i++) m.erase(i);
        auto m2 = m;
        assert(m2.size() == 25);
        for (int i = 25; i < 50; i++) assert(m2[i] == i);
    }
    
    // Test 5: Move semantics
    {
        emhash6::HashMap<int,int> m;
        for (int i = 0; i < 100; i++) m[i] = i;
        auto m2 = std::move(m);
        assert(m2.size() == 100);
        assert(m.size() == 0);
        for (int i = 0; i < 100; i++) assert(m2[i] == i);
    }
    
    // Test 6: Large size reserve then partial insert
    {
        emhash5::HashMap<int,int> m;
        m.reserve(1000000);
        for (int i = 0; i < 100; i++) m[i] = i;
        assert(m.size() == 100);
        for (int i = 0; i < 100; i++) assert(m[i] == i);
    }
    
    // Test 7: Rehash to smaller size
    {
        emhash7::HashMap<int,int> m;
        for (int i = 0; i < 1000; i++) m[i] = i;
        m.rehash(2000);
        assert(m.size() == 1000);
        for (int i = 0; i < 1000; i++) assert(m[i] == i);
    }
    
    // Test 8: Iterator after erase
    {
        emhash8::HashMap<int,int> m;
        for (int i = 0; i < 10; i++) m[i] = i;
        auto it = m.begin();
        ++it;
        m.erase(m.begin());
        assert(it != m.end());
    }
    
    // Test 9: All 4 implementations consistency
    {
        for (int seed = 0; seed < 10; seed++) {
            emhash5::HashMap<int,int> m5;
            emhash6::HashMap<int,int> m6;
            emhash7::HashMap<int,int> m7;
            emhash8::HashMap<int,int> m8;
            
            for (int i = 0; i < 500; i++) {
                int k = (i * 7 + seed) % 1000;
                m5[k] = i; m6[k] = i; m7[k] = i; m8[k] = i;
            }
            assert(m5.size() == m6.size());
            assert(m6.size() == m7.size());
            assert(m7.size() == m8.size());
            
            for (int i = 0; i < 500; i++) {
                int k = (i * 7 + seed) % 1000;
                assert(m5[k] == m6[k]);
                assert(m6[k] == m7[k]);
                assert(m7[k] == m8[k]);
            }
        }
    }
    
    // Test 10: String key edge cases
    {
        emhash5::HashMap<std::string, int> m;
        m[""] = 1;
        m["hello"] = 2;
        m["hello"] = 3;
        assert(m[""] == 1);
        assert(m["hello"] == 3);
        assert(m.size() == 2);
    }
    
    // Test 11: High load factor stress
    {
        emhash7::HashMap<int,int> m;
        m.max_load_factor(0.95f);
        m.reserve(1000);
        for (int i = 0; i < 950; i++) m[i] = i;
        assert(m.size() == 950);
        for (int i = 0; i < 950; i++) assert(m[i] == i);
    }
    
    // Test 12: Erase non-existent key
    {
        emhash5::HashMap<int,int> m;
        m[1] = 1;
        assert(m.erase(999) == 0);
        assert(m.size() == 1);
    }
    
    // Test 13: Clear and reuse
    {
        emhash6::HashMap<int,int> m;
        for (int i = 0; i < 100; i++) m[i] = i;
        m.clear();
        assert(m.size() == 0);
        for (int i = 0; i < 100; i++) m[i] = i * 2;
        assert(m.size() == 100);
        for (int i = 0; i < 100; i++) assert(m[i] == i * 2);
    }
    
    printf("All edge case tests passed!\n");
    return 0;
}
