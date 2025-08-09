#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf8_to_utf16_test_base;

  constexpr size_t trials = 1000;
}


// For invalid inputs, we expect the conversion to fail (return 0)
TEST_LOOP(trials, convert_random_inputs) {
    simdutf::tests::helpers::RandomInt r(0x00, 0xffffffff, seed);

    for (size_t size: input_size) {
      std::vector<char32_t> utf32(size);
      for(size_t i = 0; i < size; i++) {
        utf32[i] = r();
      }
      size_t buffer_size = implementation.latin1_length_from_utf32(size);
      std::vector<char> latin1(buffer_size);
      size_t actual_size = implementation.convert_utf32_to_latin1(utf32.data(), size, latin1.data());
      if(implementation.validate_utf32(utf32.data(), size)) {
        ASSERT_EQUAL(buffer_size, actual_size);
      } else {
        ASSERT_EQUAL(0, actual_size);
      }
    }
}

TEST(convert_latin1_only) {
  size_t counter = 0;
  auto generator = [&counter]() -> uint32_t {
    return counter++ & 0xFF; 
  };

  auto procedure = [&implementation](const char32_t* utf32, size_t size, char* latin1) -> size_t {
    return implementation.convert_utf32_to_latin1(utf32, size, latin1);
  };
  auto size_procedure = [](const char32_t*, size_t size) -> size_t {
    return size;
  };
  for (size_t size: input_size) {
    simdutf::tests::helpers::transcode_utf32_to_latin1_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
    ASSERT_TRUE(test.check_size(size_procedure));
  }
}

TEST_LOOP(trials, convert_fails_if_input_too_large) {
    simdutf::tests::helpers::RandomInt generator(0xFF, 0xffffffff, seed);

    auto procedure = [&implementation](const char32_t* utf32, size_t size, char* latin1) -> size_t {
      return implementation.convert_utf32_to_latin1(utf32, size, latin1); 
    };
    const size_t size = 64;
    simdutf::tests::helpers::transcode_utf32_to_latin1_test_base test([](){ return '*'; }, size+32); // create an input utf32 and reference latin1 string /w all entries = 0x2a

    for (size_t j = 0; j < 1000; j++) {
      uint32_t wrong_value = generator();
      for (size_t i=0; i < size; i++) {
        auto old = test.input_utf32[i];
        test.input_utf32[i] = wrong_value; 
        ASSERT_TRUE(test(procedure)); // the procedure should not convert anything, so its output should equal the reference string that is all 0x2a
        test.input_utf32[i] = old; 
      }
    }
}

TEST_MAIN
