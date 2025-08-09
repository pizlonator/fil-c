#include "simdutf.h"

#include <tests/helpers/random_utf8.h>
#include <tests/helpers/test.h>

constexpr size_t num_trials = 1000;

TEST(validate_utf8_with_errors_cbf29ce4842223f0) {
   const unsigned char data[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xff};
   constexpr std::size_t data_len_bytes = sizeof(data);
   constexpr std::size_t data_len = data_len_bytes / sizeof(char);
   const auto validation1 = implementation.validate_utf8_with_errors((const char *) data, data_len);
   /*
   got return [count=64, error=SUCCESS] from implementation icelake
   got return [count=63, error=HEADER_BITS] from implementation haswell
   got return [count=63, error=HEADER_BITS] from implementation westmere
   got return [count=63, error=HEADER_BITS] from implementation fallback
   */
   ASSERT_EQUAL(validation1.count, 63);
   ASSERT_EQUAL(validation1.error, simdutf::error_code::HEADER_BITS);
}


// https://github.com/nodejs/node/issues/48995
TEST(node48995) {
  const char bad[1] = {(char)0x80};
  size_t length = 1;
  simdutf::result res = implementation.validate_utf8_with_errors(bad, length);
  ASSERT_TRUE(res.error);
}

TEST(copyright) {
  const char good[2] = {'\xC2', '\xA9'};
  size_t length = 2;
  simdutf::result res = implementation.validate_utf8_with_errors(good, length);
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
}


TEST_LOOP(num_trials, no_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    const auto utf8{generator.generate(512, seed)};
    simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
    ASSERT_EQUAL(res.count, utf8.size());
}

TEST_LOOP(num_trials, header_bits_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};

    for (int i = 0; i < 512; i++) {
      if((utf8[i] & 0b11000000) != 0b10000000) {  // Only process leading bytes
        const unsigned char old = utf8[i];
        utf8[i] = uint8_t(0b11111000);
        simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ASSERT_EQUAL(res.error, simdutf::error_code::HEADER_BITS);
        ASSERT_EQUAL(res.count, i);
        utf8[i] = old;
      }
    }
}

TEST_LOOP(num_trials, too_short_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};
    int leading_byte_pos = 0;
    for (int i = 0; i < 512; i++) {
      if((utf8[i] & 0b11000000) == 0b10000000) {  // Only process continuation bytes by making them leading bytes
        const unsigned char old = utf8[i];
        utf8[i] = uint8_t(0b11100000);
        simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ASSERT_EQUAL(res.error, simdutf::error_code::TOO_SHORT);
        ASSERT_EQUAL(res.count, leading_byte_pos);
        utf8[i] = old;
      } else {
        leading_byte_pos = i;
      }
    }
}

TEST_LOOP(num_trials, too_long_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};
    for (int i = 1; i < 512; i++) {
      if(((utf8[i] & 0b11000000) != 0b10000000)) {  // Only process leading bytes by making them continuation bytes
        const unsigned char old = utf8[i];
        utf8[i] = uint8_t(0b10000000);
        simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ASSERT_EQUAL(res.error, simdutf::error_code::TOO_LONG);
        ASSERT_EQUAL(res.count, i);
        utf8[i] = old;
      }
    }
}

TEST_LOOP(num_trials, overlong_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};
    for (int i = 1; i < 512; i++) {
      if(utf8[i] >= 0b11000000) { // Only non-ASCII leading bytes can be overlong
        const unsigned char old = utf8[i];
        const unsigned char second_old = utf8[i+1];
        if ((old & 0b11100000) == 0b11000000) { // two-bytes case, change to a value less or equal than 0x7f
          utf8[i] = 0b11000000;
        } else if ((old & 0b11110000) == 0b11100000) {  // three-bytes case, change to a value less or equal than 0x7ff
          utf8[i] = 0b11100000;
          utf8[i+1] = utf8[i+1] & 0b11011111;
        } else {  // four-bytes case, change to a value less or equal than 0xffff
          utf8[i] = 0b11110000;
          utf8[i+1] = utf8[i+1] & 0b11001111;
        }
        simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ASSERT_EQUAL(res.error, simdutf::error_code::OVERLONG);
        ASSERT_EQUAL(res.count, i);
        utf8[i] = old;
        utf8[i+1] = second_old;
      }
    }
}

TEST_LOOP(num_trials, too_large_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};
    for (int i = 1; i < 512; i++) {
      if((utf8[i] & 0b11111000) == 0b11110000) { // Can only have too large error in 4-bytes case
        utf8[i] += ((utf8[i] & 0b100) == 0b100) ? 0b10 : 0b100;   // Make sure we get too large error and not header bits error
        simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        ASSERT_EQUAL(res.error, simdutf::error_code::TOO_LARGE);
        ASSERT_EQUAL(res.count, i);
        utf8[i] -= 0b100;
      }
    }
}

TEST_LOOP(num_trials, surrogate_error) {
    simdutf::tests::helpers::random_utf8 generator{seed, 1, 1, 1, 1};
    auto utf8{generator.generate(512, seed)};
    for (int i = 1; i < 512; i++) {
      if((utf8[i] & 0b11110000) == 0b11100000) { // Can only have surrogate error in 3-bytes case
        const unsigned char old = utf8[i];
        const unsigned char second_old = utf8[i+1];
        utf8[i] = 0b11101101;                 // Leading byte is always the same
        for (int s = 0x8; s < 0xf; s++) {  // Modify second byte to create a surrogate codepoint
          utf8[i+1] = (utf8[i+1] & 0b11000011) | (s << 2);
          simdutf::result res = implementation.validate_utf8_with_errors(reinterpret_cast<const char*>(utf8.data()), utf8.size());
          ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
          ASSERT_EQUAL(res.count, i);
        }
        utf8[i] = old;
        utf8[i+1] = second_old;
      }
    }
}

TEST_MAIN
