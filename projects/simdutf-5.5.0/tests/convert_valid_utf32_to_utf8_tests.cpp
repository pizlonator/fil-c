#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf32_to_utf8_test_base;

  constexpr int trials = 1000;
}

TEST(convert_pure_ASCII) {
  size_t counter = 0;
  auto generator = [&counter]() -> uint32_t {
    return counter++ & 0x7f;
  };

  auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
    return implementation.convert_valid_utf32_to_utf8(utf32, size, utf8);
  };

  std::array<size_t, 5> input_size{16, 12, 64, 128, 256};
  for (size_t size: input_size) {
    transcode_utf32_to_utf8_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
  }
}

TEST_LOOP(trials, convert_into_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf32_to_utf8(utf32, size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_into_1_or_2_or_3_UTF8_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x007f},
                                                     {0x0080, 0x07ff},
                                                     {0x0800, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf32_to_utf8(utf32, size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_into_3_or_4_UTF8_bytes) {
    // range for 3 or 4 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf32_to_utf8(utf32, size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_MAIN
