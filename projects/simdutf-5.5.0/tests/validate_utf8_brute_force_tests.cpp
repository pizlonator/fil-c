#include "simdutf.h"

#include <tests/helpers/random_utf8.h>
#include <tests/reference/validate_utf8.h>
#include <tests/helpers/test.h>

template<typename T>
static void test_corrupt(T &implementation, uint32_t seed, simdutf::tests::helpers::random_utf8 gen_utf8) {
  std::mt19937 gen(seed);
  for (size_t i = 0; i < 10; i++) {
    auto UTF8 = gen_utf8.generate(1000);
    if (!implementation.validate_utf8((const char *)UTF8.data(), UTF8.size())) {
      puts("bug");
      ASSERT_TRUE(false);
    }
    std::uniform_int_distribution<size_t> distIdx{0, UTF8.size()-1};
    for (size_t j = 0; j < 1000; ++j) {
      const size_t corrupt = distIdx(gen);
      uint8_t restore = UTF8[corrupt];
      UTF8[corrupt] = uint8_t(gen());
      bool is_ok =
          implementation.validate_utf8((const char *)UTF8.data(), UTF8.size());
      bool is_ok_basic =
          simdutf::tests::reference::validate_utf8((const char *)UTF8.data(), UTF8.size());
      if (is_ok != is_ok_basic) {
        puts("bug");
        ASSERT_TRUE(false);
      }
      UTF8[corrupt] = restore;
    }
  }
}

TEST(corrupt_1byte) {
  uint32_t seed{1234};
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 1, 0, 0, 0));
}

TEST(corrupt_2byte) {
  uint32_t seed{1234};
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 0, 1, 0, 0));
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 1, 1, 0, 0));
}

TEST(corrupt_3byte) {
  uint32_t seed{1234};
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 0, 0, 1, 0));
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 0, 1, 1, 0));
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 1, 0, 1, 0));
  test_corrupt(implementation, seed, simdutf::tests::helpers::random_utf8(seed, 1, 1, 1, 0));
}

TEST(brute_force) {
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf8 gen_1_2_3_4(seed, 1, 1, 1, 1);
  size_t total = 1000;
  for (size_t i = 0; i < total; i++) {

    auto UTF8 = gen_1_2_3_4.generate(rand() % 256);
    if (!implementation.validate_utf8((const char *)UTF8.data(), UTF8.size())) {
      puts("bug");
      ASSERT_TRUE(false);
    }
    for (size_t flip = 0; flip < 1000; ++flip) {
      // we are going to hack the string as long as it is UTF-8
      const int bitflip{1 << (rand() % 8)};
      UTF8[rand() % UTF8.size()] = uint8_t(bitflip); // we flip exactly one bit
      bool is_ok =
          implementation.validate_utf8((const char *)UTF8.data(), UTF8.size());
      bool is_ok_basic =
          simdutf::tests::reference::validate_utf8((const char *)UTF8.data(), UTF8.size());
      if (is_ok != is_ok_basic) {
        puts("bug");
        ASSERT_TRUE(false);
      }
    }
  }
}

TEST_MAIN
