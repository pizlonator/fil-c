#include "simdutf.h"

#include <array>

#include <tests/reference/validate_utf16.h>
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

  auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* utf8) -> size_t {
    std::vector<char16_t> utf16be(size);
    implementation.change_endianness_utf16(utf16le, size, utf16be.data());
    return implementation.convert_valid_utf16be_to_utf8(utf16be.data(), size, utf8);
  };

  std::array<size_t, 1> input_size{16};
  for (size_t size: input_size) {
    transcode_utf16_to_utf8_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
  }
}

TEST_LOOP(trials, convert_into_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* utf8) -> size_t {
      std::vector<char16_t> utf16be(size);
      implementation.change_endianness_utf16(utf16le, size, utf16be.data());
      return implementation.convert_valid_utf16be_to_utf8(utf16be.data(), size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_into_1_or_2_or_3_UTF8_bytes) {
    // range for 1, 2 or 3 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0x007f},
                                                     {0x0080, 0x07ff},
                                                     {0x0800, 0xd7ff},
                                                     {0xe000, 0xffff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* utf8) -> size_t {
      std::vector<char16_t> utf16be(size);
      implementation.change_endianness_utf16(utf16le, size, utf16be.data());
      return implementation.convert_valid_utf16be_to_utf8(utf16be.data(), size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

TEST_LOOP(trials, convert_into_3_or_4_UTF8_bytes) {
    // range for 3 or 4 UTF-8 bytes
    simdutf::tests::helpers::RandomIntRanges random({{0x0800, 0xd800-1},
                                                     {0xe000, 0x10ffff}}, seed);

    auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* utf8) -> size_t {
      std::vector<char16_t> utf16be(size);
      implementation.change_endianness_utf16(utf16le, size, utf16be.data());
      return implementation.convert_valid_utf16be_to_utf8(utf16be.data(), size, utf8);
    };

    for (size_t size: input_size) {
      transcode_utf16_to_utf8_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
    }
}

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
namespace {
  std::vector<std::vector<char16_t>> all_combinations() {
    const char16_t V_1byte_start  = 0x0042; // non-surrogate word the yields 1 UTF-8 byte
    const char16_t V_2bytes_start = 0x017f; // non-surrogate word the yields 2 UTF-8 bytes
    const char16_t V_3bytes_start = 0xefff; // non-surrogate word the yields 3 UTF-8 bytes
    const char16_t L        = 0xd9ca; // low surrogate
    const char16_t H        = 0xde42; // high surrogate

    std::vector<std::vector<char16_t>> result;
    std::vector<char16_t> row(32, '*');

    std::array<int, 8> pattern{0};
    while (true) {
      //if (result.size() > 5) break;

      // 1. produce output
      char16_t V_1byte = V_1byte_start;
      char16_t V_2bytes = V_2bytes_start;
      char16_t V_3bytes = V_3bytes_start;
      for (int i=0; i < 8; i++) {
        switch (pattern[i]) {
          case 0:
            row[i] = V_1byte++;
            break;
          case 1:
            row[i] = V_2bytes++;
            break;
          case 2:
            row[i] = V_3bytes++;
            break;
          case 3:
            row[i] = L;
            break;
          case 4:
            row[i] = H;
            break;
          default:
            abort();
        }
      } // for

      if (row[7] == L) {
        row[8] = H; // make input valid
        result.push_back(row);

        row[8] = V_1byte; // broken input
        result.push_back(row);
      } else {
        row[8] = V_1byte;
        result.push_back(row);
      }

      // next pattern
      int i = 0;
      int carry = 1;
      for (/**/; i < 8 && carry; i++) {
        pattern[i] += carry;
        if (pattern[i] == 5) {
          pattern[i] = 0;
          carry = 1;
        } else
          carry = 0;
      }

      if (carry == 1 and i == 8)
        break;

    } // while

    return result;
  }
}

TEST(all_possible_8_codepoint_combinations) {
  auto procedure = [&implementation](const char16_t* utf16le, size_t size, char* utf8) -> size_t {
    std::vector<char16_t> utf16be(size);
    implementation.change_endianness_utf16(utf16le, size, utf16be.data());
    return implementation.convert_valid_utf16be_to_utf8(utf16be.data(), size, utf8);
  };

  std::vector<char> output_utf8(256, ' ');
  const auto& combinations = all_combinations();
  for (const auto& input_utf16: combinations) {
    if (simdutf::tests::reference::validate_utf16(input_utf16.data(), input_utf16.size())) {
      transcode_utf16_to_utf8_test_base test(input_utf16);
      ASSERT_TRUE(test(procedure));
    }
  }
}
#endif

TEST_MAIN
