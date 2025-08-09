#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>


namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf32_to_utf16_test_base;

  constexpr int trials = 1000;
}


TEST_LOOP(trials, convert_into_2_UTF16_bytes) {
    // range for 2 UTF-16 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char16_t* utf16le) -> size_t {
      std::vector<char16_t> utf16be(size);
      size_t len = implementation.convert_utf32_to_utf16be(utf32, size, utf16be.data());
      implementation.change_endianness_utf16(utf16be.data(), size, utf16le);
      return len;
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf16_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_into_4_UTF16_bytes) {
    // range for 4 UTF-16 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x10000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char16_t* utf16le) -> size_t {
      std::vector<char16_t> utf16be(2*size);
      size_t len = implementation.convert_utf32_to_utf16be(utf32, size, utf16be.data());
      implementation.change_endianness_utf16(utf16be.data(), len, utf16le);
      return len;
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf16_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_into_2_or_4_UTF16_bytes) {
    // range for 2 or 4 UTF-16 bytes (all codepoints)
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0xd7ff},
                                                     {0xe000, 0xffff},
                                                     {0x10000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char16_t* utf16le) -> size_t {
      std::vector<char16_t> utf16be(2*size);
      size_t len = implementation.convert_utf32_to_utf16be(utf32, size, utf16be.data());
      implementation.change_endianness_utf16(utf16be.data(), len, utf16le);
      return len;
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf16_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST(convert_fails_if_there_is_surrogate) {
  auto procedure = [&implementation](const char32_t* utf32, size_t size, char16_t* utf16le) -> size_t {
    std::vector<char16_t> utf16be(2*size);
    size_t len = implementation.convert_utf32_to_utf16be(utf32, size, utf16be.data());
    implementation.change_endianness_utf16(utf16be.data(), len, utf16le);
    return len;
  };
  const size_t size = 64;
  transcode_utf32_to_utf16_test_base test([](){return '*';}, size + 32);

  for (char32_t surrogate = 0xd800; surrogate <= 0xdfff; surrogate++) {
    for (size_t i=0; i < size; i++) {
      const auto old = test.input_utf32[i];
      test.input_utf32[i] = surrogate;
      ASSERT_TRUE(test(procedure));
      test.input_utf32[i] = old;
    }
  }
}

TEST(convert_fails_if_input_too_large) {
  uint32_t seed{1234};
  simdutf::tests::helpers::RandomInt generator(0x110000, 0xffffffff, seed);

  auto procedure = [&implementation](const char32_t* utf32, size_t size, char16_t* utf16le) -> size_t {
    std::vector<char16_t> utf16be(2*size);
    size_t len = implementation.convert_utf32_to_utf16be(utf32, size, utf16be.data());
    implementation.change_endianness_utf16(utf16be.data(), len, utf16le);
    return len;
  };
  const size_t size = 64;
  transcode_utf32_to_utf16_test_base test([](){return '*';}, size+32);

  for (size_t j = 0; j < 1000; j++) {
    uint32_t wrong_value = generator();
    for (size_t i=0; i < size; i++) {
      auto old = test.input_utf32[i];
      test.input_utf32[i] = wrong_value;
      ASSERT_TRUE(test(procedure));
      test.input_utf32[i] = old;
    }
  }
}

TEST_MAIN
