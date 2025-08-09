#include "simdutf.h"

#include <array>

#include <tests/reference/validate_utf16_to_latin1.h>
#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>


namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf16_to_latin1_test_base;

  constexpr int trials = 1000;
}

// For invalid inputs, we expect the conversion to fail (return 0)
TEST_LOOP(trials, convert_random_inputs) {
    simdutf::tests::helpers::RandomInt r(0x00, 0xffff, seed);

    for (size_t size: input_size) {
      std::vector<char16_t> utf16(size);
      for(size_t i = 0; i < size; i++) {
        utf16[i] = r();
      }
      size_t buffer_size = implementation.latin1_length_from_utf16(size);
      std::vector<char> latin1(buffer_size);
      size_t actual_size = implementation.convert_utf16le_to_latin1(utf16.data(), size, latin1.data());
      if(simdutf::tests::reference::validate_utf16_to_latin1(utf16.data(), size)) {
        ASSERT_EQUAL( actual_size,buffer_size);
      } else {
        ASSERT_EQUAL(actual_size,0);
      }
    }
}


TEST_LOOP(trials, convert_randoms) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00ff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* latin1) -> size_t {
      return implementation.convert_utf16le_to_latin1(utf16, size, latin1);
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.latin1_length_from_utf16(size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_latin1_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_LOOP(trials, convert_1_or_2_UTF16_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00ff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* latin1) -> size_t {
      return implementation.convert_utf16le_to_latin1(utf16, size, latin1);
    };
    auto size_procedure = [&implementation](const char16_t* utf16, size_t size) -> size_t {
      return implementation.latin1_length_from_utf16(size);
    };
    for (size_t size: input_size) {
      transcode_utf16_to_latin1_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST(convert_fails_if_input_too_large) {
  uint32_t seed{1234};
  simdutf::tests::helpers::RandomInt generator(0xff, 0xffff, seed);

  auto procedure = [&implementation](const char16_t* utf16, size_t size, char* latin1) -> size_t {
    return implementation.convert_utf16le_to_latin1(utf16, size, latin1);
  };
  const size_t size = 64;
  transcode_utf16_to_latin1_test_base test([](){ return '*'; }, size+32);

  for (size_t j = 0; j < 1000; j++) {
    uint16_t wrong_value = generator();
    #if SIMDUTF_IS_BIG_ENDIAN // Big endian systems invert the declared generator's numbers when committed to memory.
    // Each codepoints above 255 are thus mirrored.
    // e.g. abcd becomes cdab, and vice versa. This is for most codepoints,not a cause for concern.
    // One case is however problematic, that of the numbers in the BE format 0xYY00 where the mirror image indicates a number beneath 255
    // which is undesirable in this particular test.
    if ((wrong_value & 0xFF00) != 0){
      // In this case, we swap bytes of the generated value:
      wrong_value = uint16_t((wrong_value >> 8) | (wrong_value << 8));
    }
    #endif
    for (size_t i=0; i < size; i++) {
      auto old = test.input_utf16[i];
      test.input_utf16[i] = wrong_value;
      ASSERT_TRUE(test(procedure));
      test.input_utf16[i] = old;
    }
  }
}

TEST_MAIN
