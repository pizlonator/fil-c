#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf8_to_utf32_test_base;

  constexpr size_t trials = 10000;
}

TEST_LOOP(trials, convert_pure_ASCII) {
    size_t counter = 0;
    auto generator = [&counter]() -> uint32_t {
      return counter++ & 0x7f;
    };

    auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
      return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf32_test_base test(generator, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
      return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf32_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_1_or_2_or_3_UTF8_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
      return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf32_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_3_or_4_UTF8_bytes) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed); // range for 3 or 4 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
      return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf32_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}


TEST_LOOP(trials, convert_null_4_UTF8_bytes) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00000},
                                                     {0x10000, 0x10ffff}}, seed); // range for 3 or 4 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
      return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf32_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST(issue132) {
  uint32_t seed{1234};

  // range for 2,3 and 4 UTF-8 bytes 
  simdutf::tests::helpers::RandomIntRanges random({{0x080, 0xd800-1},
                                                   {0xe000, 0x10ffff}}, seed);

  auto procedure = [&implementation](const char* utf8, size_t size, char32_t* utf32) -> size_t {
    return implementation.convert_valid_utf8_to_utf32(utf8, size, utf32);
  };

  const size_t size = 200;
  std::vector<uint32_t> data(size+32, '*');

  for (size_t j = 0; j < 1000; j++) {
    uint32_t non_ascii = random();
    for (size_t i=0; i < size; i++) {
      auto old = data[i];
      data[i] = non_ascii;
      transcode_utf8_to_utf32_test_base test(data);
      ASSERT_TRUE(test(procedure));
      data[i] = old;
    }
  }
}

TEST_MAIN
