/*
 * unordered_array_test.cxx
 * Copyright© 2017 rsw0x
 *
 * Distributed under terms of the MPLv2 license.
 */

#include "catch.hpp"
#include "unordered_array.hpp"

#include <numeric>
#include <random>

TEST_CASE("unordered_array") {
  nonstd::unordered_array<int> ua;
  std::vector<nonstd::key_t> keys;
  SECTION("one") {
    REQUIRE(ua.empty());
    auto k = ua.emplace(5);
    REQUIRE(!ua.empty());
    REQUIRE(ua.count(k) == 1);
    REQUIRE(ua.find(k) != ua.end());
    REQUIRE(ua.find(k) == ua.begin());
    ua.erase(k);

    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }
    REQUIRE(ua.count(keys[0]) == 1);
    auto fail = -1;
    for (auto i = 0; i < 100'000; ++i) {
      if (ua.count(nonstd::key_t{i}) != 1) {
        fail = i;
        break;
      }
    }
    REQUIRE(fail == -1);
    REQUIRE(ua.size() != 0);
    ua.clear();
    REQUIRE(ua.size() == 0);
  }
  SECTION("two") {
    REQUIRE(ua.empty());
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }
    ptrdiff_t n = std::accumulate(ua.begin(), ua.end(), 0ll);
    REQUIRE(n == (99'999ll * (100'000)) / 2);

    REQUIRE(keys.size() == ua.size());
  }
  SECTION("shuffle") {
    auto k0 = ua.emplace(9);
    auto k1 = ua.emplace(7);
    REQUIRE(ua[k0] == 9);
    REQUIRE(ua[k1] == 7);
    int* a0 = &ua[k0];
    int* a1 = &ua[k1];
    ua.shuffle(k0, k1);
    REQUIRE(ua[k0] == 7);
    REQUIRE(ua[k1] == 9);
    REQUIRE(a0 == &ua[k1]);
    REQUIRE(a1 == &ua[k0]);
  }
  SECTION("shuffle2") {
    auto k0 = ua.emplace(9);
    auto k1 = ua.emplace(7);
    REQUIRE(ua.at(k0) == 9);
    REQUIRE(ua.at(k1) == 7);
    int* a0 = &ua.at(k0);
    int* a1 = &ua.at(k1);
    ua.shuffle(k0, k1);
    REQUIRE(ua.at(k0) == 7);
    REQUIRE(ua.at(k1) == 9);
    REQUIRE(a0 == &ua.at(k1));
    REQUIRE(a1 == &ua.at(k0));
  }
  SECTION("insertion") {
    REQUIRE(ua.empty());
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }
    for (const auto key : keys) {
      if (ua.count(key) != 1) {
        REQUIRE(ua.count(key) == 1);
      }
    }
  }
  SECTION("erase") {
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }
    for (const auto key : keys) {
      REQUIRE(ua.count(key) == 1);
      ua.erase(key);
    }
  }
  SECTION("erase shuffle") {
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(keys.begin(), keys.end(), rng);
    for (const auto key : keys) {
      REQUIRE(ua.count(key) == 1);
      ua.erase(key);
    }
  }
  SECTION("at") {
    auto k0 = ua.emplace(0);
    REQUIRE_NOTHROW(ua.at(k0));
    ua.erase(k0);
    REQUIRE_THROWS(ua.at(k0));

    REQUIRE(ua.empty());
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(keys.begin(), keys.end(), rng);
    for (const auto key : keys) {
      REQUIRE_NOTHROW(ua.at(key));
    }
    ua.clear();
    REQUIRE(ua.empty());
    REQUIRE(ua.find(nonstd::key_t{}) == ua.end());
  }
  SECTION("random") {
    keys.reserve(100'000);
    for (auto i = 0; i < 100'000; ++i) {
      keys.emplace_back(ua.emplace(i));
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    while(!keys.empty()){
      auto idx = rng() % keys.size();
      auto key = keys[idx];
      std::swap(keys[idx], keys.back());
      keys.pop_back();
      REQUIRE(ua.count(key) == 1);
      ua.erase(key);
      REQUIRE(ua.count(key) == 0);
    }
  }
}
