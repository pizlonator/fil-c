#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_latin1_to_utf16_test_base;

  constexpr int trials = 1000;
}

TEST_LOOP(trials, convert_all_latin) {
    // range for 2 UTF-16 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x00, 0xff}}, seed);

    auto procedure = [&implementation](const char* latin1, size_t size, char16_t* utf16) -> size_t {
      return implementation.convert_latin1_to_utf16le(latin1, size, utf16);
    };
    auto size_procedure = [&implementation](const char* latin1, size_t size) -> size_t {
      return implementation.utf16_length_from_latin1( size);
    };
    for (size_t size: input_size) {
      transcode_latin1_to_utf16_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

TEST_MAIN
