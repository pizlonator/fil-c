#include "simdutf.h"

#include <array>

#include <tests/helpers/transcode_test_base.h>
#include <tests/helpers/random_int.h>
#include <tests/helpers/test.h>

namespace {
  std::array<size_t, 7> input_size{7, 16, 12, 64, 67, 128, 256};

  using simdutf::tests::helpers::transcode_utf8_to_latin1_test_base;

  constexpr int fix_size = 512;

  constexpr size_t trials = 10000;
}

void printByteInBinary(const char& byte) {
    for (int i = 7; i >= 0; --i) {
        putchar(((byte >> i) & 1) ? '1' : '0');
    }
    putchar('\n');
}

TEST(issue_convert_utf8_to_latin1_with_errors_01f0c43ba5b92120)
{
    alignas(1) const unsigned char data[] = {0xc3, 0x81, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                             0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xc3,
                                             0xb9, 0xb9};
    constexpr std::size_t data_len_bytes = sizeof(data);
    constexpr std::size_t data_len = data_len_bytes / sizeof(char);
    const auto validation1 = implementation.validate_utf8_with_errors((const char *) data, data_len);
    ASSERT_EQUAL(validation1.count, 64);
    ASSERT_EQUAL(validation1.error, simdutf::error_code::TOO_LONG);

    const bool validation2 = implementation.validate_utf8((const char *) data, data_len);
    ASSERT_EQUAL(validation1.error == simdutf::error_code::SUCCESS, validation2);

    const auto outlen = implementation.latin1_length_from_utf8((const char *) data, data_len);
    ASSERT_EQUAL(outlen, 62);
    std::vector<char> output(outlen);
    const auto r = implementation.convert_utf8_to_latin1_with_errors((const char *) data,
                                                                     data_len,
                                                                     output.data());
}

TEST(issue_483) {
    const unsigned char data[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xa9};
    constexpr std::size_t data_len_bytes = sizeof(data);
    constexpr std::size_t data_len = data_len_bytes / sizeof(char);
    const auto validation1 = implementation.validate_utf8_with_errors((const char *) data, data_len);
    ASSERT_EQUAL(validation1.count, 64);
    ASSERT_EQUAL(validation1.error, simdutf::error_code::TOO_LONG);

    const auto outlen = implementation.latin1_length_from_utf8((const char *) data, data_len);
    ASSERT_EQUAL(outlen, 64);
    std::vector<char> output(outlen);
    const auto r = implementation.convert_utf8_to_latin1_with_errors((const char *) data,
                                                                      data_len,
                                                                      output.data());
    ASSERT_EQUAL(r.error, simdutf::error_code::TOO_LONG);
    ASSERT_EQUAL(r.count, 64);
}


TEST(issue_convert_utf8_to_latin1_with_errors_a8ec246845d4878e) {
    const unsigned char data[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0xf2, 0xa8, 0xa4, 0x8b, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xf2, 0xa8,
                                  0xa4, 0x8b, 0x20, 0x20, 0x20, 0x20, 0xf2, 0xa8, 0xa4, 0x8b, 0x20,
                                  0x20, 0xf2, 0xa8, 0xa4, 0x8b, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0xf2, 0xa8, 0xa4, 0xa8, 0xa4, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
    constexpr std::size_t data_len_bytes = sizeof(data);
    constexpr std::size_t data_len = data_len_bytes / sizeof(char);
    std::vector<char> output(4 * data_len);
    const auto r = implementation.convert_utf8_to_latin1_with_errors((const char *) data,
                                                                    data_len,
                                                                    output.data());
    ASSERT_EQUAL(r.count, 13);
    ASSERT_EQUAL(r.error, simdutf::error_code::TOO_LARGE);
}

TEST(issue_convert_utf8_to_latin1_with_errors_cbf29ce4842223ed) {
   const unsigned char data[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xc2};
   constexpr std::size_t data_len_bytes = sizeof(data);
   constexpr std::size_t data_len = data_len_bytes / sizeof(char);
   std::vector<char> output(4 * data_len);
   const auto r = implementation.convert_utf8_to_latin1_with_errors((const char *) data,
                                                                    data_len,
                                                                    output.data());
   /*
   got return [count=63, error=SUCCESS] from implementation icelake
   got return [count=63, error=TOO_SHORT] from implementation haswell
   got return [count=63, error=TOO_SHORT] from implementation westmere
   got return [count=63, error=TOO_SHORT] from implementation fallback
   */
   ASSERT_EQUAL(r.count, 63);
   ASSERT_EQUAL(r.error, simdutf::error_code::TOO_SHORT);
}

TEST_LOOP(trials, convert_pure_ASCII) {
    size_t counter = 0;
    auto generator = [&counter]() -> uint8_t {
      return counter++ & 0x7f;
    };

    auto procedure = [&implementation](const char* utf8, size_t size, char* latin1) -> size_t {
      return implementation.convert_utf8_to_latin1(utf8, size, latin1);
    };
    auto size_procedure = [&implementation](const char* utf8, size_t size) -> size_t {
      return implementation.latin1_length_from_utf8(utf8, size);
    };

    for (size_t size: input_size) {
      transcode_utf8_to_latin1_test_base test(generator, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
} 

TEST_LOOP(trials, convert_1_or_2_valid_UTF8_bytes_to_latin1) {
    simdutf::tests::helpers::RandomInt random(0x0000, 0x0ff, seed); // range for 1 or 2 UTF-8 bytes

    auto procedure = [&implementation](const char* utf8, size_t size, char* latin1) -> size_t {
      return implementation.convert_utf8_to_latin1(utf8, size, latin1);
    };
    auto size_procedure = [&implementation](const char* utf8, size_t size) -> size_t {
      return implementation.latin1_length_from_utf8(utf8, size);
    };
    for (size_t size: input_size) {
      transcode_utf8_to_latin1_test_base test(random, size);
      ASSERT_TRUE(test(procedure));
      ASSERT_TRUE(test.check_size(size_procedure));
    }
}

 TEST_LOOP(trials, too_large_input) {
auto getUtf8SequenceLength = [](char byte) {
    if ((byte & 0b11100000) == 0b11000000) { // 2 byte UTF-8 header
        return 2;
    }
    else if ((byte & 0b11110000) == 0b11100000) { // 3 byte UTF-8 header
        return 3;
    }
    else if ((byte & 0b11111000) == 0b11110000) { // 4 byte UTF-8 header
        return 4;
    }
    else {
        return 1; // 1 byte UTF-8 sequence (ASCII)
    }
};

    simdutf::tests::helpers::RandomIntRanges random({{0xff, 0x10FFFF}}, seed);
    transcode_utf8_to_latin1_test_base test(random, fix_size);
    for (int i = 0; i < fix_size; i++) {
      auto byte_number = getUtf8SequenceLength(test.input_utf8[i]);
      if(byte_number != 1) { 

        auto procedure = [&implementation, &i](const char* utf8, size_t size, char* latin1) -> size_t {
          simdutf::result res = implementation.convert_utf8_to_latin1_with_errors(utf8, size, latin1);
          ASSERT_EQUAL(res.error,simdutf::error_code::TOO_LARGE);
          ASSERT_EQUAL(res.count,i); 
          return 0;
        };

        ASSERT_TRUE(test(procedure)); // no conversion should take place

        // does the same as the conditional above: e.g. replace a 4 byte by a '*' ASCII character once its done.
        for(auto it = test.input_utf8.begin(); it != test.input_utf8.end(); ++it) {
            if(std::distance(it, test.input_utf8.end()) >= byte_number) { 
                std::fill_n(it, byte_number, 0x2a);
            }
        }

        }
    }
} 

TEST_LOOP(trials, header_bits_error) {
    simdutf::tests::helpers::RandomIntRanges random({{0x0000, 0xff}}, seed);
    transcode_utf8_to_latin1_test_base test(random, fix_size);

    for (int i = 0; i < fix_size; i++) {

      if((test.input_utf8[i] & 0b11000000) != 0b10000000) {  // Only process leading bytes
        auto procedure = [&implementation, &i](const char* utf8, size_t size, char* latin1) -> size_t {
          simdutf::result res = implementation.convert_utf8_to_latin1_with_errors(utf8, size,latin1);
          ASSERT_EQUAL(res.count, i);
          ASSERT_EQUAL(res.error, simdutf::error_code::HEADER_BITS);
          return 0;
        };
        const unsigned char old = test.input_utf8[i];
        test.input_utf8[i] = uint8_t(0b11111000);
        ASSERT_TRUE(test(procedure));
        test.input_utf8[i] = old;
      }
    }
}

TEST_LOOP(trials, too_short_error) {
    simdutf::tests::helpers::RandomIntRanges random({{0x00, 0xff}}, seed);
    transcode_utf8_to_latin1_test_base test(random, fix_size);
    int leading_byte_pos = 0;
    for (int i = 0; i < fix_size; i++) {

      if((test.input_utf8[i] & 0b11000000) == 0b10000000) {  // Only process continuation bytes by making them leading bytes

        auto procedure = [&implementation, &leading_byte_pos](const char* utf8, size_t size,char * latin1) -> size_t {
          simdutf::result res = implementation.convert_utf8_to_latin1_with_errors(utf8, size, latin1);
          ASSERT_EQUAL(res.count, leading_byte_pos);
          ASSERT_EQUAL(res.error, simdutf::error_code::TOO_SHORT);
          return 0;
        };

        const unsigned char old = test.input_utf8[i];
        test.input_utf8[i] = uint8_t(0b11100000);        
        ASSERT_TRUE(test(procedure));
        test.input_utf8[i] = old;
      } else {
        leading_byte_pos = i;
      }
    }
}


TEST_LOOP(trials, too_long_error) {
    simdutf::tests::helpers::RandomIntRanges random({{0x7f, 0xff}}, seed); // in this context, conversion to latin 1 will register everything past 0xff as a TOO_LARGE error
    transcode_utf8_to_latin1_test_base test(random, fix_size);
    for (int i = 1; i < fix_size; i++) {
      if(((test.input_utf8[i] & 0b11000000) != 0b10000000)) {  // Only process leading bytes by making them continuation bytes
        auto procedure = [&implementation, &i](const char* utf8, size_t size, char* latin1) -> size_t {
          simdutf::result res = implementation.convert_utf8_to_latin1_with_errors(utf8, size, latin1);
          ASSERT_EQUAL(res.error, simdutf::error_code::TOO_LONG);
          ASSERT_EQUAL(res.count, i);
          return 0;
        };
        const unsigned char old = test.input_utf8[i];
        test.input_utf8[i] = uint8_t(0b10000000);
        ASSERT_TRUE(test(procedure));
        test.input_utf8[i] = old;
      }
    }
}


TEST_LOOP(trials, overlong_error) {
    simdutf::tests::helpers::RandomIntRanges random({{0x00, 0xff}}, seed);
    transcode_utf8_to_latin1_test_base test(random, fix_size);
    for (int i = 1; i < fix_size; i++) {
      if((unsigned char)test.input_utf8[i] >= (unsigned char)0b11000000) { // Only non-ASCII leading bytes can be overlong
        auto procedure = [&implementation, &i](const char* utf8, size_t size, char* latin1) -> size_t {
          simdutf::result res = implementation.convert_utf8_to_latin1_with_errors(utf8, size,latin1);
          ASSERT_EQUAL(res.error, simdutf::error_code::OVERLONG);
          ASSERT_EQUAL(res.count, i);
          return 0;
        };
        const unsigned char old = test.input_utf8[i];
        const unsigned char second_old = test.input_utf8[i+1];
        if ((old & 0b11100000) == 0b11000000) { // two-bytes case, change to a value less or equal than 0x7f
          test.input_utf8[i] = char(0b11000000);
        }
        ASSERT_TRUE(test(procedure));
        test.input_utf8[i] = old;
        test.input_utf8[i+1] = second_old;
      }
    }
}


TEST(issue_446) {
    const unsigned char crash[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0xc2, 0xa2, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
                                   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
    const unsigned int crash_len = 128;

    std::vector<char> output(4 * crash_len);
    const auto r = implementation.convert_utf8_to_latin1_with_errors((const char *) crash,
                                                               crash_len,
                                                               output.data());
    ASSERT_EQUAL(r.count, 127); // because of the sequence 0xc2, 0xa2
    ASSERT_EQUAL(r.error, simdutf::error_code::SUCCESS);
}

TEST_MAIN
