#include "../../../Catch2/catch.hpp"

#include <random>
#include <vector>
#include "../src/BitMaskIter.hpp"

using bit_iter = BitMaskIter64;

class RandomNumGen {
  std::random_device rd;
  std::mt19937_64 eng;
  std::uniform_int_distribution<uint64_t> dist;

 public:
  RandomNumGen() noexcept : eng(rd()) {}

  constexpr uint64_t operator()() noexcept { return dist(eng); }

} RNG;

constexpr bit_iter mask_default{};
constexpr bit_iter mask_all_set(0xFFFFFFFFFFFFFFFF);

TEST_CASE("Constructor", "[bit_iter][constructors]") {
  SECTION("Default Constructor") {
    REQUIRE(mask_default.bits_ == 0);
    REQUIRE(mask_default == bit_iter{});
    REQUIRE(mask_default == bit_iter(0));
  }
  SECTION("uint64_t Constructor") {
    constexpr int64_t i = 0xAAAABBBBCCCCDDDD;
    constexpr bit_iter mask(i);
    REQUIRE(mask.bits_ == 0xAAAABBBBCCCCDDDD);
  }
  SECTION("2xuint32_t Constructor") {
    constexpr uint32_t a = 0xAAAABBBB;
    constexpr uint32_t b = 0xCCCCDDDD;
    constexpr bit_iter mask(a, b);
    REQUIRE(mask.bits_ == 0xCCCCDDDDAAAABBBB);
  }
  SECTION("4xuint32_t Constructor") {
    constexpr uint32_t a = 0x0000AAAA;
    constexpr uint32_t b = 0x0000BBBB;
    constexpr uint32_t c = 0x0000CCCC;
    constexpr uint32_t d = 0x0000DDDD;
    bit_iter mask(a, b, c, d);
    REQUIRE(mask.bits_ == 0xDDDDCCCCBBBBAAAA);
  }
}
TEST_CASE("Operators", "[bit_iter][operators]") {
  SECTION("operator bool()") {
    constexpr bit_iter mask((uint64_t)1);
    REQUIRE(!mask_default);
    REQUIRE(mask);
  }
  SECTION("operator==, operator!=") {
    constexpr bit_iter mask1(1024);
    constexpr bit_iter mask2(1024);
    constexpr bit_iter mask3(512);
    REQUIRE(mask1 == mask2);
    REQUIRE(mask2 != mask3);
  }
  SECTION("operator++") {
    bit_iter mask(0x9000000000000000);
    ++mask;
    REQUIRE(mask.bits_ == 0x8000000000000000);
    ++mask;
    REQUIRE(mask.bits_ == 0);
  }
  SECTION("operator*") {
    REQUIRE(*mask_all_set == 0);
    REQUIRE(*mask_default == -1);
    constexpr bit_iter mask(0x8000000000000000);
    REQUIRE(*mask == 63);
  }
}
TEST_CASE("Member Functions", "[bit_iter][methods]") {
  SECTION("countTrailingZeros()") {
    REQUIRE(mask_default.countTrailingZeros() == 64);
    REQUIRE(mask_all_set.countTrailingZeros() == 0);
    constexpr bit_iter mask(0xFFFFFFFF00000000);
    REQUIRE(mask.countTrailingZeros() == 32);
  }
  SECTION("begin()") {
    constexpr bit_iter mask1((uint64_t)123456789);
    constexpr bit_iter mask2((uint64_t)123456789);
    REQUIRE(mask1.begin() == mask2);
    REQUIRE(mask1.begin() == mask2.begin());
  }
  SECTION("end()") {
    bit_iter mask(RNG());
    REQUIRE(mask.end() == mask_default);
  }
  SECTION("getFirstBit()") {
    REQUIRE(mask_default.getFirstBit() == 0);
    REQUIRE(mask_all_set.getFirstBit() == 1);
  }
  SECTION("getLastBit()") {
    REQUIRE(mask_default.getLastBit() == 0);
    REQUIRE(mask_all_set.getLastBit() == 1);
  }
  SECTION("getFirstUnsetBit()") {}
}

TEST_CASE("Range For Loop", "[bit_iter][loop]") {
  std::vector<int> set_indexes(64);
  std::vector<int> compare(64);
  SECTION("Range over default") {
    for (auto&& i : mask_default) set_indexes.emplace_back(i);
    REQUIRE(set_indexes == compare);
  }
  set_indexes.clear();
  compare.clear();
  SECTION("Range over all_set_indexes") {
    for (auto&& i : mask_all_set) set_indexes.emplace_back(i);
    for (auto i{0}; i < 64; ++i) compare.emplace_back(i);
    REQUIRE(set_indexes == compare);
  }
  set_indexes.clear();
  compare.clear();
  SECTION("Range over every other set") {
    bit_iter every_other(0xAAAAAAAAAAAAAAAA);
    for (auto&& i : every_other) set_indexes.emplace_back(i);
    for (auto i{63}; i >= 0; i -= 2) compare.insert(compare.begin(), i);
    REQUIRE(set_indexes == compare);
  }
}
