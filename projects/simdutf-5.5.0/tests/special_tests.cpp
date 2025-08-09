/**
 * Special tests. Specific cases.
 */
#include "simdutf.h"

#include <memory>
#include <tests/helpers/test.h>

// Visual Studio 2019 fails this test (with high probability) on the
// icelake kernel. Visual Studio 2022 works fine. 
TEST(visualstudio2019icelakeissue) {
  const uint16_t buf[] = {
      123,   34,    105,   100,   34,    58,    49,    44,    34,    109,
      101,   116,   104,   111,   100,   34,    58,    34,    82,    117,
      110,   116,   105,   109,   101,   46,    101,   118,   97,    108,
      117,   97,    116,   101,   34,    44,    34,    112,   97,    114,
      97,    109,   115,   34,    58,    123,   34,    101,   120,   112,
      114,   101,   115,   115,   105,   111,   110,   34,    58,    34,
      99,    111,   110,   115,   116,   32,    120,   50,    32,    61,
      32,    39,    55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341, 55357, 56341,
      39,    34,    44,    34,    116,   104,   114,   111,   119,   79,
      110,   83,    105,   100,   101,   69,    102,   102,   101,   99,
      116,   34,    58,    116,   114,   117,   101,   44,    34,    116,
      105,   109,   101,   111,   117,   116,   34,    58,    51,    51,
      51,    125,   125};
  const char16_t *source = reinterpret_cast<const char16_t *>(buf);
  const size_t length = sizeof(buf)/sizeof(char16_t); // 74 is enough
  size_t expected_size = implementation.utf8_length_from_utf16le(source, length);
  std::unique_ptr<char[]> buffer(new char[expected_size]);
  size_t out_size =
      implementation.convert_utf16le_to_utf8(source, length, buffer.get());
  std::string final_string(buffer.get(), out_size);
  ASSERT_EQUAL(expected_size, out_size);
}

TEST(special_cases_utf8_utf32_roundtrip) {
  std::string cases[] = {
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xf0\x91\x81\x80\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(validutf8);
    // We need a buffer where to write the UTF-16LE code units.
    size_t expected_utf32words =
        simdutf::utf32_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char32_t[]> utf32_output{new char32_t[expected_utf32words]};
    // convert to UTF-32
    size_t utf32words = simdutf::convert_utf8_to_utf32(
        source.c_str(), source.size(), utf32_output.get());
    // It wrote utf32words * sizeof(char32_t) bytes.
    bool validutf32 = simdutf::validate_utf32(utf32_output.get(), utf32words);
    ASSERT_TRUE(validutf32);

    std::unique_ptr<char32_t[]> utf32_valid_output{
        new char32_t[expected_utf32words]};
    // convert to UTF-16LE
    size_t utf32words_valid = simdutf::convert_valid_utf8_to_utf32(
        source.c_str(), source.size(), utf32_valid_output.get());
    ASSERT_EQUAL(utf32words_valid, utf32words);
    for (size_t z = 0; z < utf32words_valid; z++) {
      ASSERT_EQUAL(uint32_t(utf32_valid_output.get()[z]), uint32_t (utf32_output.get()[z]));
    }

    // convert it back:
    // We need a buffer where to write the UTF-8 code units.
    size_t expected_utf8words =
        simdutf::utf8_length_from_utf32(utf32_output.get(), utf32words);
    ASSERT_EQUAL(expected_utf8words, source.size());
    std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
    // convert to UTF-8
    size_t utf8words = simdutf::convert_utf32_to_utf8(
        utf32_output.get(), utf32words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words);
    std::string final_string(utf8_output.get(), utf8words);
    ASSERT_EQUAL(final_string, source);

    size_t utf8words_valid = simdutf::convert_valid_utf32_to_utf8(
        utf32_output.get(), utf32words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words_valid);
    const std::string final_string_valid(utf8_output.get(), utf8words_valid);
    ASSERT_EQUAL(final_string_valid, source);
  }
}

TEST(special_cases_utf8_utf16le_roundtrip) {
  std::string cases[] = {
      "\x05\x0A\x0A\x01\x0C\x01\x01\x0A\x0C\x01\x01\x01\x01\x01\x0A\x0A\x0A\xF0"
      "\x93\x93\x93\x01\x01\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D"
      "\x01\x7B\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D\x01\x7B\x01"
      "\x01\x01\x01\x0A\x01\x2A\x0A\x7E\x0A\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01",
      "\x20\x20\xEF\xBB\x8A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20",
      "hello\xe4\xbd\xa0\xe5\xa5\xbd",
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xf0\x91\x81\x80\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(validutf8);
    // We need a buffer where to write the UTF-16LE code units.
    size_t expected_utf16words =
        simdutf::utf16_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf16words]};
    // convert to UTF-16LE
    size_t utf16words = simdutf::convert_utf8_to_utf16le(
        source.c_str(), source.size(), utf16_output.get());
    // It wrote utf16words * sizeof(char16_t) bytes.
    bool validutf16 = simdutf::validate_utf16le(utf16_output.get(), utf16words);
    ASSERT_TRUE(validutf16);

    std::unique_ptr<char16_t[]> utf16_valid_output{
        new char16_t[expected_utf16words]};
    // convert to UTF-16LE
    size_t utf16words_valid = simdutf::convert_valid_utf8_to_utf16le(
        source.c_str(), source.size(), utf16_valid_output.get());
    ASSERT_EQUAL(utf16words_valid, utf16words);
    for (size_t z = 0; z < utf16words_valid; z++) {
      ASSERT_EQUAL(uint16_t(utf16_valid_output.get()[z]), uint16_t(utf16_output.get()[z]));
    }

    // convert it back:
    // We need a buffer where to write the UTF-8 code units.
    size_t expected_utf8words =
        simdutf::utf8_length_from_utf16le(utf16_output.get(), utf16words);
    ASSERT_EQUAL(expected_utf8words, source.size());
    std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
    // convert to UTF-8
    size_t utf8words = simdutf::convert_utf16le_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words);
    std::string final_string(utf8_output.get(), utf8words);
    ASSERT_EQUAL(final_string, source);

    size_t utf8words_valid = simdutf::convert_valid_utf16le_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words_valid);
    const std::string final_string_valid(utf8_output.get(), utf8words_valid);
    ASSERT_EQUAL(final_string_valid, source);
  }
}

TEST(special_cases_utf8_utf16be_roundtrip) {
  std::string cases[] = {
      "\x05\x0A\x0A\x01\x0C\x01\x01\x0A\x0C\x01\x01\x01\x01\x01\x0A\x0A\x0A\xF0"
      "\x93\x93\x93\x01\x01\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D"
      "\x01\x7B\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D\x01\x7B\x01"
      "\x01\x01\x01\x0A\x01\x2A\x0A\x7E\x0A\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01",
      "\x20\x20\xEF\xBB\x8A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20",
      "hello\xe4\xbd\xa0\xe5\xa5\xbd",
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xf0\x91\x81\x80\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(validutf8);
    // We need a buffer where to write the UTF-16LE code units.
    size_t expected_utf16words =
        simdutf::utf16_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf16words]};
    // convert to UTF-16BE
    size_t utf16words = simdutf::convert_utf8_to_utf16be(
        source.c_str(), source.size(), utf16_output.get());
    // It wrote utf16words * sizeof(char16_t) bytes.
    bool validutf16 = simdutf::validate_utf16be(utf16_output.get(), utf16words);
    ASSERT_TRUE(validutf16);

    std::unique_ptr<char16_t[]> utf16_valid_output{
        new char16_t[expected_utf16words]};
    // convert to UTF-16BE
    size_t utf16words_valid = simdutf::convert_valid_utf8_to_utf16be(
        source.c_str(), source.size(), utf16_valid_output.get());
    ASSERT_EQUAL(utf16words_valid, utf16words);
    for (size_t z = 0; z < utf16words_valid; z++) {
      // Note: casting due to Visual Studio's lack of operator<< for char16_t.
      ASSERT_EQUAL(uint16_t(utf16_valid_output.get()[z]), uint16_t(utf16_output.get()[z]));
    }

    // convert it back:
    // We need a buffer where to write the UTF-8 code units.
    size_t expected_utf8words =
        simdutf::utf8_length_from_utf16be(utf16_output.get(), utf16words);
    ASSERT_EQUAL(expected_utf8words, source.size());
    std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
    // convert to UTF-8
    const size_t utf8words = simdutf::convert_utf16be_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words);
    const std::string final_string(utf8_output.get(), utf8words);
    ASSERT_EQUAL(final_string, source);

    const size_t utf8words_valid = simdutf::convert_valid_utf16be_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words_valid);
    const std::string final_string_valid(utf8_output.get(), utf8words_valid);
    ASSERT_EQUAL(final_string_valid, source);
  }
}

TEST(special_cases_utf8_utf16_roundtrip) {
  std::string cases[] = {
      "\x05\x0A\x0A\x01\x0C\x01\x01\x0A\x0C\x01\x01\x01\x01\x01\x0A\x0A\x0A\xF0"
      "\x93\x93\x93\x01\x01\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D"
      "\x01\x7B\x01\x01\xE2\xBB\x9A\xEF\x9B\xBB\xEE\x81\x81\x05\x2D\x01\x7B\x01"
      "\x01\x01\x01\x0A\x01\x2A\x0A\x7E\x0A\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01",
      "\x20\x20\xEF\xBB\x8A\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20",
      "hello\xe4\xbd\xa0\xe5\xa5\xbd",
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xf0\x91\x81\x80\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(validutf8);
    // We need a buffer where to write the UTF-16LE code units.
    size_t expected_utf16words =
        simdutf::utf16_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf16words]};
    // convert to UTF-16
    size_t utf16words = simdutf::convert_utf8_to_utf16(
        source.c_str(), source.size(), utf16_output.get());
    // It wrote utf16words * sizeof(char16_t) bytes.
    bool validutf16 = simdutf::validate_utf16(utf16_output.get(), utf16words);
    ASSERT_TRUE(validutf16);

    std::unique_ptr<char16_t[]> utf16_valid_output{
        new char16_t[expected_utf16words]};
    // convert to UTF-16
    size_t utf16words_valid = simdutf::convert_valid_utf8_to_utf16(
        source.c_str(), source.size(), utf16_valid_output.get());
    ASSERT_EQUAL(utf16words_valid, utf16words);
    for (size_t z = 0; z < utf16words_valid; z++) {
      // Note: casting due to Visual Studio's lack of operator<< for char16_t.
      ASSERT_EQUAL(uint16_t(utf16_valid_output.get()[z]), uint16_t(utf16_output.get()[z]));
    }

    // convert it back:
    // We need a buffer where to write the UTF-8 code units.
    size_t expected_utf8words =
        simdutf::utf8_length_from_utf16(utf16_output.get(), utf16words);
    ASSERT_EQUAL(expected_utf8words, source.size());
    std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
    // convert to UTF-8
    size_t utf8words = simdutf::convert_utf16_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words);
    std::string final_string(utf8_output.get(), utf8words);
    ASSERT_EQUAL(final_string, source);
    size_t utf8words_valid = simdutf::convert_valid_utf16_to_utf8(
        utf16_output.get(), utf16words, utf8_output.get());
    ASSERT_EQUAL(expected_utf8words, utf8words_valid);
    std::string final_string_valid(utf8_output.get(), utf8words_valid);
    ASSERT_EQUAL(final_string_valid, source);
  }
}

TEST(special_cases_utf8_utf16_invalid) {
  std::string cases[] = {
      "\xdf\xbb\xd1\x8a\xd3\x8a\xd3\x10\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c"
      "\xd4\x8a\x8a\xd3\x8c\xd3\x8a\xd3\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3"
      "\x2f\xd3\x8a\x8a\x8a\xd3\x8a\xd3\x8a\xc3\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x8a"
      "\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3\x8a\x03\xb0\x1d\x00\x8a\xd3\x76\x2f\x8a"
      "\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4\xd3\x8c\xd3\x8a"
      "\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4\xd3\x8c\xd3\xd3"
      "\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x2f\xd3\x8a\x8a\xd3\x8c\xd3\x8a"
      "\x8a\xd3\x8c\xd3\x8a\xd3\x8a\xd3\x8a\x8a\x8a\xd3\x2f\xd3\x8a\x8a\x8a\xd3"
      "\x8a\xd3\x8a\xc3\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a"
      "\xd3\xd3\xa9\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x76\x2f\x8a\x8c\xd3\x8a\x8a\xd3"
      "\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4\xd3\x8c\xd3\xd3\x8c\xd4\x8a\x8a\xd3"
      "\x8c\xd3\x8a\x8a\xd3\x2f\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a"
      "\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\x8a\xd3\x8a\xd3\x8a\xd3"
      "\x8a\xd3\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3"
      "\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x2f\xd3\x8a\x8a\xd3\x8c\xd3\x8a"
      "\x8a\xd3\x8c\xd3\x8a\xd3\x8a\xd3\x8a\x8a\x8a\xd3\x09\x2f\xd3\x8a\x8a\x8a"
      "\xd3\x8a\xd3\x8a\xc3\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3"
      "\x8a\xd3\xd3\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x76\x2f\x8a\x8c\xd3\x8a\x8a\xd3"
      "\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4\xd3\x8c\xd3\xd3\x8c\xd4\x8a\x8a\xd3"
      "\x8c\xd3\x8a\x8a\xd3\x2f\x8c\xd3\x8a\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a"
      "\xd3\x8c\x8a\x8a\xf4\xd3\x8c\xd3\xd3\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a"
      "\xd3\x2f\xd3\x3f\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\xd3\x8a\xd3\x8a"
      "\x8a\x8a\xd3\x2f\xd3\x8a\x8a\x8a\xd3\x8a\xd3\x8a\xc3\x8a\x8a\xd3\x8a\xd3"
      "\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3\xa9\x8a\x8a\xd3\x8a\xd3\x8a"
      "\xd3\x76\x2f\x8a\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4"
      "\xd3\x8c\xd3\xd3\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x2f\xd3\x8a\x8a"
      "\xd3\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3"
      "\x8a\xd3\x8a\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3\x8a\xd3\x8a\xd3\x8a\xd3"
      "\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3\x8c\xd4\x8a\x8a\xd3\x8c\xd3\x8a\x8a"
      "\xd3\x2f\x8a\x8a\xd3\xd3\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\xd3\x8a\xd3\x8a"
      "\x8a\x8a\xd3\x09\x2f\xd3\x8a\x8a\x8a\xd3\x8a\xd3\x8a\xc3\x8a\x8a\xd3\x8a"
      "\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\x8a\xd3\xd3\x8a\x8a\xd3\x8a\xd3\x8a"
      "\xd3\x76\x2f\x8a\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\x8a\x8a\xf4"
      "\xd3\x8c\xd3\xd3\x8c\xd4\xd3\x8a\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a"
      "\x76\x2f\x8a\xd3\x8a\xd3\x8a\xd3\x8c\xd3\x8a\xd3\x8c\xd3\x8a\x8a\xd3\x8c"
      "\x8c\xd3\x8a\x8a\xd3\x8c\xd3\x8a\xd3\x8a\x88",
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x05\x01\x01\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x05\x01\x01\x01\x01\x01\x01\x05\xe2\x81\x9f\xf0\x93\x93\x93\xf0\x9d\x93"
      "\x97\xf0\x93\x93\x93\xf0\x93\x91\x83\x6c",
      "\x05\x01\x0a\xf0\x91\x81\x80\x01\x26\x0a\x0a\x0a\x26\x0a\x0a\x0a\xf0\x91"
      "\x81\x80\x01\x05\x7b\x01\x17\x01\x01\x01\x01\x01\x01\x01\x26\x01\x0a\x0a"
      "\x01\x01\x0a\x11\x2a\x01\x01\x26\x0d\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xd3\x96\x2c\x2d\x2c\x01"
      "\x5b\x2d\x26\xc3\x8a\xf2\x93\x91\x91\xf3\x93\x93\x93\x26\x0d\xd3\x96\x2c"
      "\x2d\x2c\x01\x0a\x01\x01\x01\x24\xf0\x93\x97\x93\xf0\x93\x9d\x89\x0a\x01"
      "\xd3\x8a\x01\x01\x01\x01\x01\xf0\x93\x97\x93\xf0\x93\x9d\x89\x0a\x01\x01"
      "\x01\x24\x01\x01\x01\x01\xed\x9f\xb9\x0a\x0a\x01\x34\x01\x01\x01\x01\x01"
      "\x09\x01\x01\x01\x01\x01\x01\x01\x0f\x1f\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\xed\x9f\xb9\x25\xef\x93\x9f\xe8\x9f\xbf\x01\x0a\x41\xf3\x93"
      "\x93\x93\xf0\x93\x8f\x8f\x74\x01\x01\xf0\x93\x8f\xaf\xf3\x97\x91\x91\xf3"
      "\x93\x93\x93\xf0\x93\x8f\x8f\xf3\x93\x91\x91\x24\x01\x01\x6d\x01\x01\x01"
      "\x57\x57\x57\x57\x32\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37"
      "\x37\x37\x37\x37\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57"
      "\x57\x57\x57\x57\x57\x57\x56\x57\x57\xb0\xa8\xa8\xa8\x57\x57\x57\x57\x57"
      "\x57\x57\x2c\x57\x57\x57\x57\x57\x57\x57\xff\xff\xff",
      "\x20\x20\x20\x20\x20\x81\x20\xbf\xbf\x20\x20\x20\x20\xbf\x20\xb9\x83\x20"
      "\x20\x20\x20\x20\x20\x20\x20\xa6\xa6\xa6\x20\x20\x20\x20\x20\x20\x20\x20"
      "\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6"
      "\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6"
      "\xa6\xa6\xa6\xa6\xa6\xa6\x20\x20",
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xff\x20\x20\x20\x93\x20"
      "\x20\x81\x83\x20\x20\x20\x20\x20\x20\x20\x20\x20\xba\xba\xba\xa2\xa2\xa2"
      "\xa2\xa2\xba\xba\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xba"
      "\xba\xba\xba\xa2\xa2\xba\xba\xba\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2"
      "\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(!validutf8);

    size_t expected_utf16words =
        simdutf::utf16_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf16words]};
    // convert to UTF-16LE, this will fail!!! (on purpose)
    size_t utf16words = simdutf::convert_utf8_to_utf16le(
        source.c_str(), source.size(), utf16_output.get());
    ASSERT_EQUAL(utf16words, 0);

    size_t expected_latin1words =
        simdutf::latin1_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char[]> latin1_output{new char[expected_latin1words]};
    size_t latin1words = simdutf::convert_utf8_to_latin1(
          source.c_str(), source.size(), latin1_output.get());
    ASSERT_EQUAL(latin1words, 0);
  }
}

TEST(special_cases_utf8_utf32_invalid) {
  std::string cases[] = {
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x01\x05\x01\x01\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe"
      "\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\xbe\x01\x01\x01\x01\x01"
      "\x01\x01\x01\x01\x01\x05\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x05\x01\x01\x01\x01\x01\x01\x05\xe2\x81\x9f\xf0\x93\x93\x93\xf0\x9d\x93"
      "\x97\xf0\x93\x93\x93\xf0\x93\x91\x83\x6c",
      "\x05\x01\x0a\xf0\x91\x81\x80\x01\x26\x0a\x0a\x0a\x26\x0a\x0a\x0a\xf0\x91"
      "\x81\x80\x01\x05\x7b\x01\x17\x01\x01\x01\x01\x01\x01\x01\x26\x01\x0a\x0a"
      "\x01\x01\x0a\x11\x2a\x01\x01\x26\x0d\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad"
      "\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xad\xd3\x96\x2c\x2d\x2c\x01"
      "\x5b\x2d\x26\xc3\x8a\xf2\x93\x91\x91\xf3\x93\x93\x93\x26\x0d\xd3\x96\x2c"
      "\x2d\x2c\x01\x0a\x01\x01\x01\x24\xf0\x93\x97\x93\xf0\x93\x9d\x89\x0a\x01"
      "\xd3\x8a\x01\x01\x01\x01\x01\xf0\x93\x97\x93\xf0\x93\x9d\x89\x0a\x01\x01"
      "\x01\x24\x01\x01\x01\x01\xed\x9f\xb9\x0a\x0a\x01\x34\x01\x01\x01\x01\x01"
      "\x09\x01\x01\x01\x01\x01\x01\x01\x0f\x1f\x01\x01\x01\x01\x01\x01\x01\x01"
      "\x01\x01\x01\xed\x9f\xb9\x25\xef\x93\x9f\xe8\x9f\xbf\x01\x0a\x41\xf3\x93"
      "\x93\x93\xf0\x93\x8f\x8f\x74\x01\x01\xf0\x93\x8f\xaf\xf3\x97\x91\x91\xf3"
      "\x93\x93\x93\xf0\x93\x8f\x8f\xf3\x93\x91\x91\x24\x01\x01\x6d\x01\x01\x01"
      "\x57\x57\x57\x57\x32\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37\x37"
      "\x37\x37\x37\x37\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57\x57"
      "\x57\x57\x57\x57\x57\x57\x56\x57\x57\xb0\xa8\xa8\xa8\x57\x57\x57\x57\x57"
      "\x57\x57\x2c\x57\x57\x57\x57\x57\x57\x57\xff\xff\xff",
      "\x20\x20\x20\x20\x20\x81\x20\xbf\xbf\x20\x20\x20\x20\xbf\x20\xb9\x83\x20"
      "\x20\x20\x20\x20\x20\x20\x20\xa6\xa6\xa6\x20\x20\x20\x20\x20\x20\x20\x20"
      "\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6"
      "\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6\xa6"
      "\xa6\xa6\xa6\xa6\xa6\xa6\x20\x20",
      "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\xff\x20\x20\x20\x93\x20"
      "\x20\x81\x83\x20\x20\x20\x20\x20\x20\x20\x20\x20\xba\xba\xba\xa2\xa2\xa2"
      "\xa2\xa2\xba\xba\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xba"
      "\xba\xba\xba\xa2\xa2\xba\xba\xba\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2"
      "\xa2\xa2\xa2\xa2\xa2\xa2\xa2\xa2"};
  for (const std::string &source : cases) {
    bool validutf8 = simdutf::validate_utf8(source.c_str(), source.size());
    ASSERT_TRUE(!validutf8);

    size_t expected_utf32words =
        simdutf::utf32_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char32_t[]> utf32_output{new char32_t[expected_utf32words]};
    // convert to UTF-16LE, this will fail!!! (on purpose)
    size_t utf32words = simdutf::convert_utf8_to_utf32(
        source.c_str(), source.size(), utf32_output.get());
    ASSERT_EQUAL(utf32words, 0);

    size_t expected_latin1words =
        simdutf::latin1_length_from_utf8(source.c_str(), source.size());
    std::unique_ptr<char[]> latin1_output{new char[expected_latin1words]};
    size_t latin1words = simdutf::convert_utf8_to_latin1(
          source.c_str(), source.size(), latin1_output.get());
    ASSERT_EQUAL(latin1words, 0);
  }
}



TEST(special_cases_utf16_utf8_roundtrip) {
  std::vector<char16_t> cases[] = {
    {105, 109, 112, 111, 114, 116, 32, 34, 47, 118, 97, 114, 47, 102, 111, 108,
    100, 101, 114, 115, 47, 104, 100, 47, 107, 95, 99, 120, 108, 57, 102, 53, 53,
    52, 55, 53, 113, 54, 50, 109, 56, 95, 102, 95, 57, 107, 95, 99, 48, 48, 48,
    48, 103, 110, 47, 84, 47, 98, 117, 110, 45, 116, 101, 115, 116, 45, 110, 111,
    110, 45, 101, 110, 103, 108, 105, 115, 104, 45, 105, 109, 112, 111, 114, 116,
    45, 105, 109, 112, 111, 114, 116, 115, 47, 55357, 56832, 55357, 56832, 55357,
    56832, 55357, 56832, 45, 117, 116, 102, 49, 54, 45, 112, 114, 101, 102, 105,
    120, 46, 106, 115, 34, 59, 10, 105, 109, 112, 111, 114, 116, 32, 34, 47, 118,
    97, 114, 47, 102, 111, 108, 100, 101, 114, 115, 47, 104, 100, 47, 107, 95, 99,
    120, 108, 57, 102, 53, 53, 52, 55, 53, 113, 54, 50, 109, 56, 95, 102, 95, 57,
    107, 95, 99, 48, 48, 48, 48, 103, 110, 47, 84, 47, 98, 117, 110, 45, 116, 101,
    115, 116, 45, 110, 111, 110, 45, 101, 110, 103, 108, 105, 115, 104, 45, 105,
    109, 112, 111, 114, 116, 45, 105, 109, 112, 111, 114, 116, 115, 47, 117, 116,
    102, 49, 54, 45, 115, 117, 102, 102, 105, 120, 45, 55357, 56832, 55357, 56832,
    55357, 56832, 55357, 56832, 46, 106, 115, 34, 59, 10, 105, 109, 112, 111, 114,
    116, 32, 34, 47, 118, 97, 114, 47, 102, 111, 108, 100, 101, 114, 115, 47, 104,
    100, 47, 107, 95, 99, 120, 108, 57, 102, 53, 53, 52, 55, 53, 113, 54, 50, 109,
    56, 95, 102, 95, 57, 107, 95, 99, 48, 48, 48, 48, 103, 110, 47, 84, 47, 98,
    117, 110, 45, 116, 101, 115, 116, 45, 110, 111, 110, 45, 101, 110, 103, 108,
    105, 115, 104, 45, 105, 109, 112, 111, 114, 116, 45, 105, 109, 112, 111, 114,
    116, 115, 47, 199, 199, 199, 199, 45, 108, 97, 116, 105, 110, 49, 45, 112,
    114, 101, 102, 105, 120, 46, 106, 115, 34, 59, 10, 105, 109, 112, 111, 114,
    116, 32, 34, 47, 118, 97, 114, 47, 102, 111, 108, 100, 101, 114, 115, 47, 104,
    100, 47, 107, 95, 99, 120, 108, 57, 102, 53, 53, 52, 55, 53, 113, 54, 50, 109,
    56, 95, 102, 95, 57, 107, 95, 99, 48, 48, 48, 48, 103, 110, 47, 84, 47, 98,
    117, 110, 45, 116, 101, 115, 116, 45, 110, 111, 110, 45, 101, 110, 103, 108,
    105, 115, 104, 45, 105, 109, 112, 111, 114, 116, 45, 105, 109, 112, 111, 114,
    116, 115, 47, 108, 97, 116, 105, 110, 49, 45, 115, 117, 102, 102, 105, 120,
    45, 199, 199, 199, 199, 46, 106, 115, 34, 59}};
  for (std::vector<char16_t> &source : cases) {
    // make sure to copy so that we can detect overruns.
    std::unique_ptr<char16_t[]> utf16_input{new char16_t[source.size()]};
    memcpy(utf16_input.get(), source.data(), source.size() * sizeof(char16_t));

    bool validutf16 = simdutf::validate_utf16(utf16_input.get(), source.size());
    ASSERT_TRUE(validutf16);

    size_t expected_utf8words =
        simdutf::utf8_length_from_utf16(utf16_input.get(), source.size());
    std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
    size_t utf8words = simdutf::convert_utf16_to_utf8(
        utf16_input.get(), source.size(), utf8_output.get());
    ASSERT_EQUAL(utf8words, expected_utf8words);
    std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf8words]};
    size_t expected_utf16words =
        simdutf::utf16_length_from_utf8(utf8_output.get(), utf8words);
    size_t utf16words = simdutf::convert_utf8_to_utf16(
        utf8_output.get(), utf8words, utf16_output.get());
    ASSERT_EQUAL(utf16words, expected_utf16words);
    ASSERT_EQUAL(source.size(), expected_utf16words);
    for (size_t z = 0; z < utf16words; z++) {
      ASSERT_EQUAL(uint16_t(utf16_output.get()[z]), uint16_t(utf16_input.get()[z]));
    }
  }
}

TEST_MAIN
