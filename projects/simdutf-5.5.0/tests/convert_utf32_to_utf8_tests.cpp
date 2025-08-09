#include "simdutf.h"

#include <array>
#include <memory>

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
    return implementation.convert_utf32_to_utf8(utf32, size, utf8);
  };
  auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
    return implementation.utf8_length_from_utf32(utf32, size);
  };
  std::array<size_t, 4> input_size{7,16,24,67};
  for (size_t size: input_size) {
    transcode_utf32_to_utf8_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
    ASSERT_TRUE(test.check_size(size_procedure));
  }
}

TEST_LOOP(trials, convert_into_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_utf32_to_utf8(utf32, size, utf8);
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf8_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_into_1_or_2_or_3_UTF8_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x007f},
                                                     {0x0080, 0x07ff},
                                                     {0x0800, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_utf32_to_utf8(utf32, size, utf8);
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf8_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_into_3_or_4_UTF8_bytes) {
    // range for 3 or 4 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
      return implementation.convert_utf32_to_utf8(utf32, size, utf8);
    };
    auto size_procedure = [&implementation](const char32_t* utf32, size_t size) -> size_t {
      return implementation.utf8_length_from_utf32(utf32, size);
    };
    for (size_t size: input_size) {
      transcode_utf32_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST(convert_fails_if_there_is_surrogate) {
  auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
    return implementation.convert_utf32_to_utf8(utf32, size, utf8);
  };
  const size_t size = 64;
  transcode_utf32_to_utf8_test_base test([](){return '*';}, size + 32);

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

  auto procedure = [&implementation](const char32_t* utf32, size_t size, char* utf8) -> size_t {
    return implementation.convert_utf32_to_utf8(utf32, size, utf8);
  };
  const size_t size = 64;
  transcode_utf32_to_utf8_test_base test([](){return '*';}, size+32);

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

TEST(special_cases) {
  const uint32_t utf32[] = {0x0000, 0x0054, 0x0001, 0x0000, 0x0000, 0x0007, 0x005d, 0x027f, 0x001a};
  const char expected[] = "\x00\x54\x01\x00\x00\x07\x5d\xc9\xbf\x1a";
  size_t utf8len = implementation.utf8_length_from_utf32((const char32_t*)utf32, 9);
  std::unique_ptr<char[]> utf8(new char[utf8len]);
  size_t utf8size = implementation.convert_utf32_to_utf8((const char32_t*)utf32, 9, utf8.get());
  for(size_t i = 0; i < utf8len; i++) {
    ASSERT_EQUAL(utf8[i], expected[i]);
  }
  ASSERT_EQUAL(utf8size, utf8len);
}

TEST_MAIN
