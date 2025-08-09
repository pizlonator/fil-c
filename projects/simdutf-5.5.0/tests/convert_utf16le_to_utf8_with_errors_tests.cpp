#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>


namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf16_to_utf8_test_base;

  constexpr int trials = 1000;
}

TEST(convert_pure_ASCII) {
  size_t counter = 0;
  auto generator = [&counter]() -> uint32_t {
    return counter++ & 0x7f;
  };

  auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
    simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    return res.count;
  };
  auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
    return implementation.utf8_length_from_utf16le(utf16, size);
  };
  std::array<size_t, 1> input_size{16};
  for (size_t size: input_size) {
    transcode_utf16_to_utf8_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
    ASSERT_TRUE(test.check_size(size_procedure));
  }
}

TEST_LOOP(trials, convert_into_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
      ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
      return res.count;
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.utf8_length_from_utf16le(utf16, size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
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

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
      ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
      return res.count;
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.utf8_length_from_utf16le(utf16, size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_into_3_or_4_UTF8_bytes) {
    // range for 3 or 4 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
      ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
      return res.count;
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.utf8_length_from_utf16le(utf16, size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(convert_fails_if_there_is_sole_low_surrogate) {
  const size_t size = 64;
  transcode_utf16_to_utf8_test_base test([](){return '*';}, size + 32);

  for (char16_t low_surrogate = 0xdc00; low_surrogate <= 0xdfff; low_surrogate++) {
    for (size_t i=0; i < size; i++) {
      auto procedure = [&implementation, &i](const char16_t* utf16, size_t size, char* utf8) -> size_t {
        simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
        ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
        ASSERT_EQUAL(res.count, i);
        return 0;
      };
      const auto old = test.input_utf16[i];
      test.input_utf16[i] = low_surrogate;
      ASSERT_TRUE(test(procedure));
      test.input_utf16[i] = old;
    }
  }
}
#endif

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(convert_fails_if_there_is_sole_high_surrogate) {
  const size_t size = 64;
  transcode_utf16_to_utf8_test_base test([](){return '*';}, size + 32);

  for (char16_t high_surrogate = 0xdc00; high_surrogate <= 0xdfff; high_surrogate++) {
    for (size_t i=0; i < size; i++) {
      auto procedure = [&implementation, &i](const char16_t* utf16, size_t size, char* utf8) -> size_t {
        simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
        ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
        ASSERT_EQUAL(res.count, i);
        return 0;
      };
      const auto old = test.input_utf16[i];
      test.input_utf16[i] = high_surrogate;
      ASSERT_TRUE(test(procedure));
      test.input_utf16[i] = old;
    }
  }
}
#endif

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(convert_fails_if_there_is_low_surrogate_is_followed_by_another_low_surrogate) {
  const size_t size = 64;
  transcode_utf16_to_utf8_test_base test([](){return '*';}, size + 32);

  for (char16_t low_surrogate = 0xdc00; low_surrogate <= 0xdfff; low_surrogate++) {
    for (size_t i=0; i < size - 1; i++) {
      auto procedure = [&implementation, &i](const char16_t* utf16, size_t size, char* utf8) -> size_t {
        simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
        ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
        ASSERT_EQUAL(res.count, i);
        return 0;
      };
      const auto old0 = test.input_utf16[i + 0];
      const auto old1 = test.input_utf16[i + 1];
      test.input_utf16[i + 0] = low_surrogate;
      test.input_utf16[i + 1] = low_surrogate;
      ASSERT_TRUE(test(procedure));
      test.input_utf16[i + 0] = old0;
      test.input_utf16[i + 1] = old1;
    }
  }
}
#endif

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(convert_fails_if_there_is_surrogate_pair_is_followed_by_high_surrogate) {
  const size_t size = 64;
  transcode_utf16_to_utf8_test_base test([](){return '*';}, size + 32);

  const char16_t low_surrogate = 0xd801;
  const char16_t high_surrogate = 0xdc02;
  for (size_t i=0; i < size - 2; i++) {
    auto procedure = [&implementation, &i](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      simdutf::result res = implementation.convert_utf16le_to_utf8_with_errors(utf16, size, utf8);
      ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
      ASSERT_EQUAL(res.count, i+2);
      return 0;
    };
    const auto old0 = test.input_utf16[i + 0];
    const auto old1 = test.input_utf16[i + 1];
    const auto old2 = test.input_utf16[i + 2];
    test.input_utf16[i + 0] = low_surrogate;
    test.input_utf16[i + 1] = high_surrogate;
    test.input_utf16[i + 2] = high_surrogate;
    ASSERT_TRUE(test(procedure));
    test.input_utf16[i + 0] = old0;
    test.input_utf16[i + 1] = old1;
    test.input_utf16[i + 2] = old2;
  }
}
#endif

TEST_MAIN
