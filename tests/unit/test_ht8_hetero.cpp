// unit/test_ht8_hetero.cpp
// Heterogeneous lookup for emhash8::HashMap: a std::string-keyed map looked up
// with std::string_view / const char* / char-array keys without constructing a
// temporary std::string.
//
// Requires a transparent hasher + transparent equality (is_transparent on both,
// the C++20 unordered-container convention), e.g.
//   emhash8::HashMap<std::string, int, emhash8::string_hash, std::equal_to<>>
//
// Run under multiple configs:
//   g++ -I include -I tests -I tests/common tests/unit/test_ht8_hetero.cpp
//   ... plus -DEMH_POD_HASH=1 / -DEMH_WYHASH_HASH=1 / -DEMH_INT_HASH=1
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "emhash/hash_table8.hpp"

using TMap = emhash8::HashMap<std::string, int, emhash8::string_hash, std::equal_to<>>;

static uint64_t rng_state = 0xDEADBEEFull;
static uint64_t rng() {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static std::string make_key(int i) {
    return "key_" + std::to_string(i);
}

TEST_CASE("hash consistency across all string-family forms") {
    TMap m;
    const std::string s = "the_same_text";
    const std::string_view sv = s;
    const char* cs = s.c_str();
    char buf[32];
    strcpy(buf, s.c_str());

    CHECK(m.hash(s) == m.hash(sv));
    CHECK(m.hash(s) == m.hash(cs));
    CHECK(m.hash(s) == m.hash(buf));
#if !EMH_WYHASH_HASH
    // With EMH_WYHASH_HASH the map hashes string-family keys with its own
    // wyhashstr pipeline and never calls HashT for them, so the standalone
    // hasher's value may legitimately differ from m.hash(); within-map
    // consistency across key forms (asserted above) is the contract.
    CHECK(m.hash(s) == emhash8::string_hash{}(s));
#endif
}

TEST_CASE("heterogeneous find / contains / count / at / try_get") {
    TMap m;
    for (int i = 0; i < 500; ++i)
        m.emplace(make_key(i), i);

    SUBCASE("std::string_view keys") {
        const std::string key42 = make_key(42); // must outlive the view
        const std::string_view sv = key42;
        REQUIRE(m.find(sv) != m.end());
        CHECK(m.find(sv)->second == 42);
        CHECK(m.contains(sv));
        CHECK(m.count(sv) == 1);
        CHECK(m.at(sv) == 42);
        CHECK(m.try_get(sv) != nullptr);
        int out = 0;
        CHECK(m.try_get(sv, out));
        CHECK(out == 42);
        const TMap& cm = m;
        CHECK(cm.find(sv)->second == 42);
        CHECK(cm.at(sv) == 42);
        CHECK(cm.try_get(sv) != nullptr);
    }

    SUBCASE("const char* and char-array keys") {
        char buf[32];
        strcpy(buf, "key_7");
        REQUIRE(m.find(buf) != m.end());
        CHECK(m.find(buf)->second == 7);
        CHECK(m.find("key_7")->second == 7);
        CHECK(m.contains("key_7"));
        CHECK(m.count("key_7") == 1);
        CHECK(m.at("key_7") == 7);
        CHECK(m.try_get("key_7") != nullptr);
        CHECK(*m.try_get("key_7") == 7);
        const TMap& cm = m;
        CHECK(cm.contains("key_7"));
    }

    SUBCASE("misses do not find and do not insert") {
        const std::string_view miss = "definitely_absent";
        CHECK(m.find(miss) == m.end());
        CHECK_FALSE(m.contains(miss));
        CHECK(m.count(miss) == 0);
        CHECK(m.try_get(miss) == nullptr);
        CHECK_THROWS_AS(m.at(miss), std::out_of_range);
        CHECK(m.size() == 500);
    }

    SUBCASE("precomputed-hash overloads accept alternative forms") {
        const std::string_view sv = "key_100";
        const uint64_t h = m.hash(sv);
        CHECK(m.find(sv, h)->second == 100);
        CHECK(m.contains(sv, h));
        CHECK(m.count(sv, h) == 1);
        CHECK(m.at(sv, h) == 100);
        const char* cs = "key_200";
        CHECK(m.find(cs, m.hash(cs))->second == 200);
    }
}

TEST_CASE("heterogeneous erase") {
    TMap m;
    for (int i = 0; i < 200; ++i)
        m.emplace(make_key(i), i);

    CHECK(m.erase(std::string_view("key_10")) == 1);
    CHECK(m.erase("key_11") == 1);
    char buf[16];
    strcpy(buf, "key_12");
    CHECK(m.erase(buf) == 1);
    CHECK(m.size() == 197);
    CHECK_FALSE(m.contains("key_10"));
    CHECK_FALSE(m.contains(make_key(11)));
    CHECK_FALSE(m.contains("key_12"));

    // erase with a precomputed hash of an alternative key form
    const std::string_view sv = "key_13";
    CHECK(m.erase(sv, m.hash(sv)) == 1);
    CHECK(m.size() == 196);
    CHECK(m.erase("key_13") == 0);
}

TEST_CASE("heterogeneous lookups are safe on an empty map") {
    TMap m;
    const std::string_view sv = "anything";
    CHECK(m.find(sv) == m.end());
    CHECK_FALSE(m.contains(sv));
    CHECK(m.count(sv) == 0);
    CHECK(m.try_get(sv) == nullptr);
    CHECK_THROWS_AS(m.at(sv), std::out_of_range);
    CHECK(m.erase(sv) == 0);
    // then the map becomes usable
    m[std::string(sv)] = 5;
    CHECK(m.at(sv) == 5);
}

TEST_CASE("differential stress: string keys looked up via string_view") {
    std::unordered_map<std::string, int> oracle;
    TMap m;
    for (int op = 0; op < 20000; ++op) {
        const std::string k = make_key((int)(rng() % 10000));
        const uint64_t r = rng();
        if (r % 3 == 0) {
            oracle[k] = (int)(r >> 8);
            m[k] = (int)(r >> 8);
        } else if (r % 3 == 1) {
            CHECK(m.erase(std::string_view(k)) == oracle.erase(k));
        } else {
            const std::string_view sv = k;
            auto it = m.find(sv);
            auto oi = oracle.find(k);
            CHECK((it == m.end()) == (oi == oracle.end()));
            if (it != m.end())
                CHECK(it->second == oi->second);
        }
    }
    CHECK(m.size() == oracle.size());
    for (auto& kv : oracle) {
        auto it = m.find(std::string_view(kv.first));
        REQUIRE(it != m.end());
        CHECK(it->second == kv.second);
    }
}

TEST_CASE("collision-heavy keys still resolve through heterogeneous lookups") {
    struct WeakHash {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const noexcept { return static_cast<size_t>(sv.size()); }
        size_t operator()(const std::string& s) const noexcept { return (*this)(std::string_view(s)); }
        size_t operator()(const char* s) const noexcept { return (*this)(std::string_view(s)); }
    };
    emhash8::HashMap<std::string, int, WeakHash, std::equal_to<>> m;
    for (int i = 0; i < 4000; ++i)
        m.emplace("k" + std::to_string(i), i); // 4000 keys over ~6 hash values
    CHECK(m.size() == 4000);
    for (int i = 0; i < 4000; ++i) {
        auto it = m.find(std::string_view("k" + std::to_string(i)));
        REQUIRE(it != m.end());
        CHECK(it->second == i);
    }
    CHECK(m.erase(std::string_view("k5")) == 1);
    CHECK(m.size() == 3999);
}

TEST_CASE("non-transparent maps keep working through the conversion path") {
    emhash8::HashMap<std::string, int> m; // std::hash<std::string> + std::equal_to<std::string>
    m["a"] = 1;
    m["b"] = 2;
    // NB: erase(std::string_view) does NOT compile on a non-transparent map
    // (string_view -> std::string is an explicit constructor), same as before;
    // erase by the key itself works:
    CHECK(m.erase(std::string("a")) == 1);
    CHECK(m.size() == 1);
    // a std::string_view-keyed map looked up with std::string
    emhash8::HashMap<std::string_view, int> sm;
    std::string owned = "view_key";
    sm[std::string_view(owned)] = 9;
    CHECK(sm.at(owned) == 9);
    CHECK(sm.find(std::string("view_key")) != sm.end());
}

TEST_CASE("sv-keyed map looked up by std::string and const char*") {
    emhash8::HashMap<std::string_view, int, emhash8::string_hash, std::equal_to<>> m;
    std::string k0 = "alpha";
    std::string k1 = "beta";
    m[k0] = 0;
    m[k1] = 1;
    CHECK(m.find(std::string("alpha"))->second == 0);
    CHECK(m.find("beta")->second == 1);
    CHECK(m.contains(std::string_view("gamma")) == false);
}
