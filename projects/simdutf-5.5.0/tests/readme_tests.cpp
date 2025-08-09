/**
 * Special tests. Specific cases.
 */
#include "simdutf.h"
#include <memory>
#include <iostream>

#include <tests/helpers/test.h>

TEST(base64_fun) {
  std::cout << "==== base64_fun ====\n" << std::endl;
  std::vector<std::string> sources = {
      "  A  A  ", "  A  A  G  A  /  v  8  ", "  A  A  G  A  /  v  8  =  ", "  A  A  G  A  /  v  8  =  =  "};
  std::vector<std::vector<uint8_t>> expected = {
      {0}, {0, 0x1, 0x80, 0xfe, 0xff}, {0, 0x1, 0x80, 0xfe, 0xff}, {}}; // last one is in error
  for(size_t i = 0; i < sources.size(); i++) {
    const std::string &source = sources[i];
    std::cout << "source: '" << source << "'" << std::endl;
    // allocate enough memory for the maximal binary length
    std::vector<uint8_t> buffer(simdutf::maximal_binary_length_from_base64(
       source.data(), source.size()));
    // convert to binary and check for errors
    simdutf::result r = simdutf::base64_to_binary(
        source.data(), source.size(), (char*)buffer.data());
    if(r.error != simdutf::error_code::SUCCESS) {
      ASSERT_TRUE(expected[i].empty());
      std::cout << "output: error" << std::endl;
    } else {
      buffer.resize(r.count); // in case of success, r.count contains the output length
      ASSERT_TRUE(buffer == expected[i]);
      std::cout << "output: " << r.count << " bytes" << std::endl;
    }
  }
}


TEST(base64_fun_safe) {
  std::cout << "==== base64_fun ====\n" << std::endl;
  std::vector<std::string> sources = {
      "  A  A  ", "  A  A  G  A  /  v  8  ", "  A  A  G  A  /  v  8  =  ", "  A  A  G  A  /  v  8  =  =  "};
  std::vector<std::vector<uint8_t>> expected = {
      {0}, {0, 0x1, 0x80, 0xfe, 0xff}, {0, 0x1, 0x80, 0xfe, 0xff}, {}}; // last one is in error
  for(size_t i = 0; i < sources.size(); i++) {
    const std::string &source = sources[i];
    std::cout << "source: '" << source << "'" << std::endl;
    // allocate enough memory for the maximal binary length
    std::vector<uint8_t> buffer(simdutf::maximal_binary_length_from_base64(
       source.data(), source.size()));
    // convert to binary and check for errors
    size_t output_length = buffer.size();
    simdutf::result r = simdutf::base64_to_binary_safe(
        source.data(), source.size(), (char*)buffer.data(), output_length);
    if(r.error != simdutf::error_code::SUCCESS) {
      ASSERT_TRUE(expected[i].empty());
      std::cout << "output: error" << std::endl;
    } else {
      buffer.resize(output_length); // in case of success, output_length contains the output length
      ASSERT_TRUE(buffer == expected[i]);
      std::cout << "output: " << output_length << " bytes" << std::endl;
      std::cout << "input (consumed): " << r.count << " bytes" << std::endl;
    }
  }
}

// this is a compile test
void check_simdutf_result() {
  simdutf::result r;
}

// this is a compile test
int main_demo() {
  const char *source = "1234";
  // 4 == strlen(source)
  bool validutf8 = simdutf::validate_utf8(source, 4);
  if (validutf8) {
    puts("valid UTF-8");
  } else {
    puts("invalid UTF-8");
    return EXIT_FAILURE;
  }
  // We need a buffer where to write the UTF-16LE code units.
  size_t expected_utf16words = simdutf::utf16_length_from_utf8(source, 4);
  std::unique_ptr<char16_t[]> utf16_output{new char16_t[expected_utf16words]};
  // convert to UTF-16LE
  size_t utf16words =
      simdutf::convert_utf8_to_utf16le(source, 4, utf16_output.get());
  printf("wrote %zu UTF-16LE code units.", utf16words);
  // It wrote utf16words * sizeof(char16_t) bytes.
  bool validutf16 = simdutf::validate_utf16le(utf16_output.get(), utf16words);
  if (validutf16) {
    puts("valid UTF-16LE");
  } else {
    puts("invalid UTF-16LE");
    return EXIT_FAILURE;
  }
  // convert it back:
  // We need a buffer where to write the UTF-8 code units.
  size_t expected_utf8words =
      simdutf::utf8_length_from_utf16le(utf16_output.get(), utf16words);
  std::unique_ptr<char[]> utf8_output{new char[expected_utf8words]};
  // convert to UTF-8
  size_t utf8words = simdutf::convert_utf16le_to_utf8(
      utf16_output.get(), utf16words, utf8_output.get());
  printf("wrote %zu UTF-8 code units.", utf8words);
  std::string final_string(utf8_output.get(), utf8words);
  puts(final_string.c_str());
  if (final_string != source) {
    puts("bad conversion");
    return EXIT_FAILURE;
  } else {
    puts("perfect round trip");
  }
  return EXIT_SUCCESS;
}

TEST(utf8_streaming) {
  const char unicode[] = "\xc3\xa9\x63ole d'\xc3\xa9t\xc3\xa9";
  // suppose you want to decode only the start of this string.
  size_t length = 10;
  // Picking 10 bytes is problematic because we might end up in the middle of a
  // code point. But we can rewind to the previous code point.
  length = simdutf::trim_partial_utf8(unicode, length);
  // Now we can transcode safely
  size_t budget_utf16 = simdutf::utf16_length_from_utf8(unicode, length);
  std::unique_ptr<char16_t[]> utf16{new char16_t[budget_utf16]};
  size_t utf16words =
      simdutf::convert_utf8_to_utf16le(unicode, length, utf16.get());
  // We can then transcode the next batch
  const char * next = unicode + length;
  size_t next_length = sizeof(unicode) - length;
  size_t next_budget_utf16 = simdutf::utf16_length_from_utf8(next, next_length);
  std::unique_ptr<char16_t[]> next_utf16{new char16_t[next_budget_utf16]};
  size_t next_utf16words =
      simdutf::convert_utf8_to_utf16le(next, next_length, next_utf16.get());
  ASSERT_EQUAL(next_utf16words, next_budget_utf16);
  ASSERT_EQUAL(utf16words, budget_utf16);
}


TEST(utf16_streaming) {
  // We have three sequences of surrogate pairs (UTF-16).
  alignas(char16_t) const char unicode_char[] = "\x3c\xd8\x10\xdf\x3c\xd8\x10\xdf\x3c\xd8\x10\xdf";
  const char16_t * unicode = reinterpret_cast<const char16_t *>(unicode_char);
  // suppose you want to decode only the start of this string.
  size_t length = 3;
  // Picking 3 units is problematic because we might end up in the middle of a
  // surrogate pair. But we can rewind to the previous code point.
  length = simdutf::trim_partial_utf16le(unicode, length);
  // Now we can transcode safely
  size_t budget_utf8 = simdutf::utf8_length_from_utf16le(unicode, length);
  std::unique_ptr<char[]> utf8{new char[budget_utf8]};
  size_t utf8words =
      simdutf::convert_utf16le_to_utf8(unicode, length, utf8.get());
  // We can then transcode the next batch
  const char16_t * next = unicode + length;
  size_t next_length = 6 - length;
  size_t next_budget_utf8 = simdutf::utf8_length_from_utf16le(next, next_length);
  std::unique_ptr<char[]> next_utf8{new char[next_budget_utf8]};
  size_t next_utf8words =
      simdutf::convert_utf16le_to_utf8(next, next_length, next_utf8.get());
  ASSERT_EQUAL(next_utf8words, next_budget_utf8);
  ASSERT_EQUAL(utf8words, budget_utf8);
}

TEST(error_location_badascii) {
  // this ASCII string has a bad byte at index 5
  std::string bad_ascii = "\x20\x20\x20\x20\x20\xff\x20\x20\x20";
  simdutf::result res = implementation.validate_ascii_with_errors(bad_ascii.data(), bad_ascii.size());
  if (res.error != simdutf::error_code::SUCCESS) {
    printf("error at index %zu\n", res.count);
  }
  ASSERT_EQUAL(res.error, simdutf::error_code::TOO_LARGE);
  ASSERT_EQUAL(res.count, 5);
}


TEST(error_location_badutf8) {
  // this UTF-8 string has a bad byte at index 5
  std::string bad_utf8 = "\xc3\xa9\xc3\xa9\x20\xff\xc3\xa9";
  simdutf::result res = implementation.validate_utf8_with_errors(bad_utf8.data(), bad_utf8.size());
  if(res.error != simdutf::error_code::SUCCESS) {
    printf("error at index %zu\n", res.count);
  }
  ASSERT_EQUAL(res.error, simdutf::error_code::HEADER_BITS);
  ASSERT_EQUAL(res.count, 5);
  res = implementation.validate_utf8_with_errors(bad_utf8.data(), res.count);
  if(res.error == simdutf::error_code::SUCCESS) {
    printf("we have transcoded %zu valud bytes", res.count);
  }
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, 5);
}


TEST(error_location_badutf8_transcoding) {
  // this UTF-8 string has a bad byte at index 5
  std::string bad_utf8 = "\xc3\xa9\xc3\xa9\x20\xff\xc3\xa9";
  size_t budget_utf16 = simdutf::utf16_length_from_utf8(bad_utf8.data(), bad_utf8.size());
  std::unique_ptr<char16_t[]> utf16{new char16_t[budget_utf16]};

  simdutf::result res = simdutf::convert_utf8_to_utf16_with_errors(bad_utf8.data(), bad_utf8.size(), utf16.get());

  if(res.error != simdutf::error_code::SUCCESS) {
    printf("error at index %zu\n", res.count);
  }
  ASSERT_EQUAL(res.error, simdutf::error_code::HEADER_BITS);
  ASSERT_EQUAL(res.count, 5);
  res = simdutf::convert_utf8_to_utf16_with_errors(bad_utf8.data(), res.count, utf16.get());
  if(res.error == simdutf::error_code::SUCCESS) {
    printf("we have transcoded %zu characters", res.count);
  }
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, 3);
}

TEST_MAIN
