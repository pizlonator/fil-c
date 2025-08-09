#include "simdutf.h"

#ifndef SIMDUTF_IS_BIG_ENDIAN
#error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif

#include <array>
#include <memory>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 9> input_size{7, 12, 16, 64, 67, 128, 256, 511, 1000};

  using simdutf::tests::helpers::transcode_utf8_to_utf16_test_base;

  constexpr size_t trials = 10000;
}

TEST_LOOP(trials, convert_pure_ASCII) {
    size_t counter = 0;
    auto generator = [&counter]() -> uint32_t {
      return counter++ & 0x7f;
    };

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(generator, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_1_or_2_or_3_UTF8_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_3_UTF8_bytes) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1}}, seed); // range for 3 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_3_or_4_UTF8_bytes) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed); // range for 3 or 4 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_null_4_UTF8_bytes) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00000},
                                                     {0x10000, 0x10ffff}}, seed); // range for 3 or 4 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_valid_utf8_to_utf16le(utf8, size, utf16);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port this test for big-endian platforms.
#else
TEST(issue111) {
  // We stick to ASCII for our source code given that there is no universal way to specify the character encoding of
  // the source files.
  char16_t input[] = u"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\u30b3aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  size_t utf16_len = sizeof(input) / sizeof(char16_t) - 1;
  ASSERT_TRUE(implementation.validate_utf16le(input, utf16_len));
  ASSERT_EQUAL(implementation.utf8_length_from_utf16le(input, utf16_len), 2 + utf16_len);
  size_t utf8_len = implementation.utf8_length_from_utf16le(input, utf16_len);
  std::unique_ptr<char[]> utf8_buffer{new char[utf8_len]};
  ASSERT_EQUAL(implementation.convert_valid_utf16le_to_utf8(input, utf16_len, utf8_buffer.get()), utf8_len);

  std::unique_ptr<char16_t[]> utf16_buffer{new char16_t[utf16_len]};

  ASSERT_EQUAL(implementation.convert_valid_utf8_to_utf16le(utf8_buffer.get(), utf8_len, utf16_buffer.get()), utf16_len);

  ASSERT_EQUAL(std::char_traits<char16_t>::compare(input, utf16_buffer.get(), utf16_len), 0);
}
#endif


TEST(special_cases) {
  const uint8_t utf8[] = {0xC2, 0xA9}; // copyright sign
  const uint8_t expected[] = {0xA9, 0x00}; // expected UTF-16LE
  size_t utf16len = implementation.utf16_length_from_utf8((const char*)utf8, 2);
  ASSERT_EQUAL(utf16len, 1);
  std::unique_ptr<char16_t[]> utf16(new char16_t[utf16len]);
  size_t utf16size = implementation.convert_valid_utf8_to_utf16le((const char*)utf8, 2, utf16.get());
  ASSERT_EQUAL(utf16size, utf16len);
  ASSERT_EQUAL(memcmp((const char*)utf16.get(), expected, 2), 0);
}

TEST_MAIN
