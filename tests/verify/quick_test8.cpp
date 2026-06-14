// Quick correctness test for emhash8 merge-read optimization
#include <cstdio>
#include <cassert>
#include <unordered_map>
#include "emhash/hash_table8.hpp"

struct ConstHasher { size_t operator()(int x) const { return 0; } };
struct R4Hasher   { size_t operator()(int x) const { return x & 3; } };
struct IdHasher   { size_t operator()(int x) const { return x; } };

template<class H>
int run(const char* tag) {
    using M = emhash8::HashMap<int, int, H>;
    M m;

    // 1. basic
    for (int i = 0; i < 1000; i++) m.insert_unique(i, i * 2);
    for (int i = 0; i < 1000; i++) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i * 2) {
            printf("[%s] FAIL basic find i=%d\n", tag, i); return 1;
        }
    }

    // 2. random erase
    for (int i = 0; i < 1000; i += 2) m.erase(i);
    for (int i = 1; i < 1000; i += 2) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i * 2) {
            printf("[%s] FAIL odd-after-erase i=%d\n", tag, i); return 1;
        }
    }
    for (int i = 0; i < 1000; i += 2) {
        if (m.find(i) != m.end()) {
            printf("[%s] FAIL erased-still-found i=%d\n", tag, i); return 1;
        }
    }

    // 3. reinsert
    for (int i = 0; i < 1000; i += 2) m.insert_unique(i, i * 3);
    for (int i = 0; i < 1000; i += 2) {
        auto it = m.find(i);
        if (it == m.end() || it->second != i * 3) {
            printf("[%s] FAIL reinsert i=%d val=%d\n", tag, i, it->second); return 1;
        }
    }

    // 4. compare against unordered_map
    std::unordered_map<int, int> ref;
    for (auto it = m.begin(); it != m.end(); ++it) ref[it->first] = it->second;
    for (int i = 0; i < 1000; i++) {
        if (i % 2 == 0 && (ref.count(i) == 0 || ref[i] != i * 3)) {
            printf("[%s] FAIL ref i=%d\n", tag, i); return 1;
        }
        if (i % 2 == 1 && (ref.count(i) == 0 || ref[i] != i * 2)) {
            printf("[%s] FAIL ref odd i=%d\n", tag, i); return 1;
        }
    }

    // 5. clear
    m.clear();
    if (m.find(100) != m.end()) {
        printf("[%s] FAIL post-clear\n", tag); return 1;
    }

    printf("[%s] PASS\n", tag);
    return 0;
}

int main() {
    int rc = 0;
    printf("starting const test...\n"); fflush(stdout);
    rc |= run<ConstHasher>("const");
    printf("starting range4 test...\n"); fflush(stdout);
    rc |= run<R4Hasher>("range4");
    printf("starting id test...\n"); fflush(stdout);
    rc |= run<IdHasher>("id");
    printf("starting std test...\n"); fflush(stdout);
    rc |= run<std::hash<int>>("std");
    printf("all done rc=%d\n", rc);
    return rc;
}
