#include "../../../Catch2/catch.hpp"

#include "../src/simd_hash_table.hpp"

#include <string>

TEST_CASE("Metadata Enum Comparisons") {
  metadata empty_ = md_states::mdEmpty;
  metadata deleted_ = md_states::mdDeleted;
  metadata sentry_ = md_states::mdSentry;
  metadata full_ = 0;
  SECTION("isFull") {
    REQUIRE(isFull(full_) == true);
    REQUIRE(isFull(empty_) == false);
    REQUIRE(isFull(deleted_) == false);
    REQUIRE(isFull(sentry_) == false);
  }
  SECTION("isEmpty") {
    REQUIRE(isEmpty(empty_) == true);
    REQUIRE(isEmpty(full_) == false);
    REQUIRE(isEmpty(sentry_) == false);
    REQUIRE(isEmpty(deleted_) == false);
  }
  SECTION("isDeleted") {
    REQUIRE(isDeleted(deleted_) == true);
    REQUIRE(isDeleted(full_) == false);
    REQUIRE(isDeleted(sentry_) == false);
    REQUIRE(isDeleted(empty_) == false);
  }
  SECTION("isSenty") {
    REQUIRE(isSentry(sentry_) == true);
    REQUIRE(isSentry(full_) == false);
    REQUIRE(isSentry(deleted_) == false);
    REQUIRE(isSentry(empty_) == false);
  }
  SECTION("isEmptyOrDeleted") {
    REQUIRE(isEmptyOrDeleted(empty_) == true);
    REQUIRE(isEmptyOrDeleted(deleted_) == true);
    REQUIRE(isEmptyOrDeleted(sentry_) == false);
    REQUIRE(isEmptyOrDeleted(full_) == false);
  }
}

TEST_CASE("mdkv_group") {
  SECTION("hasSentry") {
    mdkv_group<simd_hash_table<int, int>::value_type> group;
    REQUIRE(group.hasSentry() == true);
    SECTION("Changed First element") {
      group.md_[0] = 1;
      REQUIRE(group.hasSentry() == false);
    }
  }
}

TEST_CASE("simd_hash_table") {
  const simd_hash_table<int, int> default_table_int;
  const simd_hash_table<std::string, std::string> default_table_string;

  SECTION("hash_function") {
    REQUIRE(std::hash<int>{}(42) == default_table_int.hash_function()(42));
    REQUIRE(std::hash<std::string>{}("test") ==
            default_table_string.hash_function()("test"));
  }
  SECTION("size") { REQUIRE(default_table_int.size() == 0); }
  SECTION("empty") { REQUIRE(default_table_int.empty() == true); }
  SECTION("max_size") { REQUIRE(default_table_int.max_size() == (64 * 4)); }
  SECTION("load_factor") { REQUIRE(default_table_int.load_factor() == 0.0f); }
}
