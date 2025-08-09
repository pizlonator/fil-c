#include "simdutf.h"

#include <array>

#include <tests/reference/validate_utf16.h>
#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 9> input_size{7, 12, 16, 64, 67, 128, 256, 511, 1000};

  using simdutf::tests::helpers::transcode_utf16_to_utf8_test_base;

  constexpr int trials = 1000;
}


#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(issue_a73) {
  const char16_t utf16[] =
      u"\ubced\uf799\u8794\uf0a6\u8a83\uf0bb\uad82\ufb82\u9797\u9bf0\uadaf"
      u"\ua0e0\u85e0\ueab5\uf2a2\uacab\ub7ed\ue0a8\uac8c\ue657\uf3bd\uaeb6"
      u"\u87ac\ubb39\ua719\ua5ed\uc09f\ue080\u869e\u87f0\ua99a\ub7fd\uff88"
      u"\u0cad\ufa98\u98a1\u92e0\uc29a\ud790\ua6b1\ua4f9\uf3b7\u97b5\ua495"
      u"\uadf0\ua6b5\uc6ab\uf885\uf7ae\ub7bf\ue99d\uebbd\ued87\ua2ac\ua5f0"
      u"\ub29b\ua5f7\u9080\u8eec\u84da\u4ebd\uf9ad\u868f\u81f9\u8fbb\ue9d9"
      u"\uf0b1\ua183\uc0b8\uc688\ub6f3\uc094\u3482\u97c4\uc792\ue089\u9898"
      u"\u9af2\uaf81\ued9e\ub7b9\uadf7\u9abd\ue041\ub79c\ubed0\ub8b5\u8cf0"
      u"\ub79b\uf3de\uba8e\u98f7\u8190\uafcc\ub2ff\uf4b1\u8e89\ua6fb\ua8e1"
      u"\u8cfc\u96f8\u94ac\u8cf2\u85a9\ubef2\ufcbd\ufa9d\ucdbc\ufb6d\ufe8d"
      u"\ue3a5\uf284\ua8ba\u86a8\uf89b\u8bac\uaefb\u8589\u8af3\ua08f\uba3b"
      u"\u80f2\ue0b0\ua79e\u9eff\u9e80\ua9f4\u8db1\ua8c0\ub7ee\u8df4\u928e"
      u"\ud386\ua28d\u86fc\u9182\u1bd9\uedb5\ubaac\u9cf5\ub69e\u98d6\uedbd"
      u"\u9da9\u88e0\uf290\ua1ae\u93fb\ubbc7\u8aee\ua3f8\u85bd\u9bf1\ud3b8"
      u"\ubcb8\uadd8\uff89\u919b\u8a77\u8bfa\uafad\ub6cf\ub5c6\uf096\ubd8f"
      u"\uceae\ua8ab\u81f0\ub194\ua4c0\ua4c0\ub2f6\ub8a5\u9ff3\u3cbd\u81f4"
      u"\u82ae\u9efc\ufe88\ufabe\u9980\uf9b1\u8e95\u80df\ubdf6\ub4ad";
  size_t len = sizeof(utf16) / sizeof(char16_t) - 1;
  size_t expected_length = implementation.utf8_length_from_utf16le(utf16, len);
  std::vector<char> output(expected_length);
  const char expected[] =
      "\xeb\xb3\xad\xef\x9e\x99\xe8\x9e\x94\xef\x82\xa6\xe8\xaa\x83\xef\x82\xbb"
      "\xea\xb6\x82\xef\xae\x82\xe9\x9e\x97\xe9\xaf\xb0\xea\xb6\xaf\xea\x83\xa0"
      "\xe8\x97\xa0\xee\xaa\xb5\xef\x8a\xa2\xea\xb2\xab\xeb\x9f\xad\xee\x82\xa8"
      "\xea\xb2\x8c\xee\x99\x97\xef\x8e\xbd\xea\xba\xb6\xe8\x9e\xac\xeb\xac\xb9"
      "\xea\x9c\x99\xea\x97\xad\xec\x82\x9f\xee\x82\x80\xe8\x9a\x9e\xe8\x9f\xb0"
      "\xea\xa6\x9a\xeb\x9f\xbd\xef\xbe\x88\xe0\xb2\xad\xef\xaa\x98\xe9\xa2\xa1"
      "\xe9\x8b\xa0\xec\x8a\x9a\xed\x9e\x90\xea\x9a\xb1\xea\x93\xb9\xef\x8e\xb7"
      "\xe9\x9e\xb5\xea\x92\x95\xea\xb7\xb0\xea\x9a\xb5\xec\x9a\xab\xef\xa2\x85"
      "\xef\x9e\xae\xeb\x9e\xbf\xee\xa6\x9d\xee\xae\xbd\xee\xb6\x87\xea\x8a\xac"
      "\xea\x97\xb0\xeb\x8a\x9b\xea\x97\xb7\xe9\x82\x80\xe8\xbb\xac\xe8\x93\x9a"
      "\xe4\xba\xbd\xef\xa6\xad\xe8\x9a\x8f\xe8\x87\xb9\xe8\xbe\xbb\xee\xa7\x99"
      "\xef\x82\xb1\xea\x86\x83\xec\x82\xb8\xec\x9a\x88\xeb\x9b\xb3\xec\x82\x94"
      "\xe3\x92\x82\xe9\x9f\x84\xec\x9e\x92\xee\x82\x89\xe9\xa2\x98\xe9\xab\xb2"
      "\xea\xbe\x81\xee\xb6\x9e\xeb\x9e\xb9\xea\xb7\xb7\xe9\xaa\xbd\xee\x81\x81"
      "\xeb\x9e\x9c\xeb\xbb\x90\xeb\xa2\xb5\xe8\xb3\xb0\xeb\x9e\x9b\xef\x8f\x9e"
      "\xeb\xaa\x8e\xe9\xa3\xb7\xe8\x86\x90\xea\xbf\x8c\xeb\x8b\xbf\xef\x92\xb1"
      "\xe8\xba\x89\xea\x9b\xbb\xea\xa3\xa1\xe8\xb3\xbc\xe9\x9b\xb8\xe9\x92\xac"
      "\xe8\xb3\xb2\xe8\x96\xa9\xeb\xbb\xb2\xef\xb2\xbd\xef\xaa\x9d\xec\xb6\xbc"
      "\xef\xad\xad\xef\xba\x8d\xee\x8e\xa5\xef\x8a\x84\xea\xa2\xba\xe8\x9a\xa8"
      "\xef\xa2\x9b\xe8\xae\xac\xea\xbb\xbb\xe8\x96\x89\xe8\xab\xb3\xea\x82\x8f"
      "\xeb\xa8\xbb\xe8\x83\xb2\xee\x82\xb0\xea\x9e\x9e\xe9\xbb\xbf\xe9\xba\x80"
      "\xea\xa7\xb4\xe8\xb6\xb1\xea\xa3\x80\xeb\x9f\xae\xe8\xb7\xb4\xe9\x8a\x8e"
      "\xed\x8e\x86\xea\x8a\x8d\xe8\x9b\xbc\xe9\x86\x82\xe1\xaf\x99\xee\xb6\xb5"
      "\xeb\xaa\xac\xe9\xb3\xb5\xeb\x9a\x9e\xe9\xa3\x96\xee\xb6\xbd\xe9\xb6\xa9"
      "\xe8\xa3\xa0\xef\x8a\x90\xea\x86\xae\xe9\x8f\xbb\xeb\xaf\x87\xe8\xab\xae"
      "\xea\x8f\xb8\xe8\x96\xbd\xe9\xaf\xb1\xed\x8e\xb8\xeb\xb2\xb8\xea\xb7\x98"
      "\xef\xbe\x89\xe9\x86\x9b\xe8\xa9\xb7\xe8\xaf\xba\xea\xbe\xad\xeb\x9b\x8f"
      "\xeb\x97\x86\xef\x82\x96\xeb\xb6\x8f\xec\xba\xae\xea\xa2\xab\xe8\x87\xb0"
      "\xeb\x86\x94\xea\x93\x80\xea\x93\x80\xeb\x8b\xb6\xeb\xa2\xa5\xe9\xbf\xb3"
      "\xe3\xb2\xbd\xe8\x87\xb4\xe8\x8a\xae\xe9\xbb\xbc\xef\xba\x88\xef\xaa\xbe"
      "\xe9\xa6\x80\xef\xa6\xb1\xe8\xba\x95\xe8\x83\x9f\xeb\xb7\xb6\xeb\x92"
      "\xad";
  size_t utf8size =
      implementation.convert_valid_utf16le_to_utf8(utf16, len, output.data());
  ASSERT_EQUAL(utf8size, expected_length);
  for (size_t i = 0; i < expected_length; i++) {
    ASSERT_EQUAL(output[i], expected[i]);
  }
}
#endif

#if SIMDUTF_IS_BIG_ENDIAN
// todo: port the next test.
#else
TEST(issue_a72) {
  const char16_t utf16[] =
      u"\ufb8f\ua092\ub1cb\ufd99\u0dad\ud2be\u87ab\u88f0\u8a88\ua127\ua4f9"
      u"\uaf9d\ufeca\u8095\u90dc\ud497\uc0b3\ud6b1\ueda5\ubca4\ubfd8\ue98a"
      u"\uf2ba\ua8a3\u7e85\ufcbc\u9b83\ub7ed\ueda7\u99bb\u94e1\u91ff";
  const char expected[] =
      "\xef\xae\x8f\xea\x82\x92\xeb\x87\x8b\xef\xb6\x99\xe0\xb6\xad\xed\x8a\xbe"
      "\xe8\x9e\xab\xe8\xa3\xb0\xe8\xaa\x88\xea\x84\xa7\xea\x93\xb9\xea\xbe\x9d"
      "\xef\xbb\x8a\xe8\x82\x95\xe9\x83\x9c\xed\x92\x97\xec\x82\xb3\xed\x9a\xb1"
      "\xee\xb6\xa5\xeb\xb2\xa4\xeb\xbf\x98\xee\xa6\x8a\xef\x8a\xba\xea\xa2\xa3"
      "\xe7\xba\x85\xef\xb2\xbc\xe9\xae\x83\xeb\x9f\xad\xee\xb6\xa7\xe9\xa6\xbb"
      "\xe9\x93\xa1\xe9\x87\xbf";
  char utf8[96];
  size_t utf8size = implementation.convert_valid_utf16le_to_utf8(utf16, 32, utf8);
  ASSERT_EQUAL(utf8size, 96);
  for (size_t i = 0; i < 96; i++) {
    ASSERT_EQUAL(utf8[i], expected[i]);
  }
}
#endif

TEST(convert_pure_ASCII) {
  size_t counter = 0;
  auto generator = [&counter]() -> uint32_t {
    return counter++ & 0x7f;
  };

  auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
    return implementation.convert_valid_utf16le_to_utf8(utf16, size, utf8);
  };

  std::array<size_t, 1> input_size{16};
  for (size_t size: input_size) {
    transcode_utf16_to_utf8_test_base test(generator, size);
    ASSERT_TRUE(test(procedure));
  }
}

TEST_LOOP(trials, convert_into_1_or_2_UTF8_bytes) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x07ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf16le_to_utf8(utf16, size, utf8);
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

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf16le_to_utf8(utf16, size, utf8);
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

    auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
      return implementation.convert_valid_utf16le_to_utf8(utf16, size, utf8);
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
  auto procedure = [&implementation](const char16_t* utf16, size_t size, char* utf8) -> size_t {
    return implementation.convert_valid_utf16le_to_utf8(utf16, size, utf8);
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
