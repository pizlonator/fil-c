#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf16_to_latin1_test_base;

  constexpr int trials = 1000;
}

TEST_LOOP(trials, convert_2_UTF16_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x00ff}, }, seed);

    auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* latin1) -> size_t {
    std::vector<char16_t> utf16be(size);
    implementation.change_endianness_utf16(utf16le, size, utf16be.data());

      return implementation.convert_valid_utf16be_to_latin1(utf16be.data(), size, latin1);
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

TEST_MAIN
