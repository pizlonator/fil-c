#include <fuzzer/FuzzedDataProvider.h>
#include <memory>
#include <string>
#include <iostream>

#include "simdutf.h"

// useful for debugging
static void print_input(const std::string& s, const simdutf::implementation *const e) {
  printf("We are about to abort on the following input: ");
  for (auto c : s) {
    printf("%02x ", (unsigned char)c);
  }
  printf("\n");
  std::cout << "string length : " << s.size() << " bytes" << std::endl;
  std::cout << "implementation->name() = " << e->name() << std::endl;
}

/**
 * We do round trips from UTF-8 to UTF-16, from UTF-8 to UTF-32, from UTF-16 to
 * UTF-8.
 * We do round trips from Latin 1 to UTF-8, from Latin 1 to UTF-16, from Latin 1 to UTF-32.
 * We test all available kernels.
 * We also try to transcode invalid inputs.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  constexpr int kMaxStringSize = 1024;
  std::string source = fdp.ConsumeRandomLengthString(kMaxStringSize);
  for (auto &e : simdutf::get_available_implementations()) {
    if (!e->supported_by_runtime_system()) {
      continue;
    }
    /**
     * Transcoding from UTF-8 to UTF-16LE.
     */
    bool validutf8 = e->validate_utf8(source.c_str(), source.size());
    auto rutf8 = e->validate_utf8_with_errors(source.c_str(), source.size());
    if(validutf8 != (rutf8.error == simdutf::SUCCESS)) { // they should agree
      print_input(source, e);
      abort();
    }
    if (validutf8) {
      // We need a buffer of size where to write the UTF-16LE words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-16LE
      size_t utf16words = e->convert_utf8_to_utf16le(
          source.c_str(), source.size(), utf16_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf16 = e->validate_utf16le(utf16_output.get(), utf16words);
      if (!validutf16) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the UTF-8 words.
      size_t expected_utf8words =
          e->utf8_length_from_utf16le(utf16_output.get(), utf16words);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      // convert to UTF-8
      size_t utf8words = e->convert_utf16le_to_utf8(
          utf16_output.get(), utf16words, utf8_output.get());
      std::string final_string(utf8_output.get(), utf8words);
      if (final_string != source) {
        print_input(source, e);
        abort();
      }
    } else {
      // invalid input!!!
      // We need a buffer of size where to write the UTF-16LE words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-16LE
      size_t utf16words = e->convert_utf8_to_utf16le(
          source.c_str(), source.size(), utf16_output.get());
      if (utf16words != 0) {
        print_input(source, e);
        abort();
      }
    }

    /**
     * Transcoding from UTF-8 to UTF-16BE.
     */
    if (validutf8) {
      // We need a buffer of size where to write the UTF-16BE words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-16BE
      size_t utf16words = e->convert_utf8_to_utf16be(
          source.c_str(), source.size(), utf16_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf16 = e->validate_utf16be(utf16_output.get(), utf16words);
      if (!validutf16) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the UTF-8 words.
      size_t expected_utf8words =
          e->utf8_length_from_utf16be(utf16_output.get(), utf16words);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      // convert to UTF-8
      size_t utf8words = e->convert_utf16be_to_utf8(
          utf16_output.get(), utf16words, utf8_output.get());
      std::string final_string(utf8_output.get(), utf8words);
      if (final_string != source) {
        print_input(source, e);
        abort();
      }
    } else {
      // invalid input!!!
      // We need a buffer of size where to write the UTF-16BE words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-16BE
      size_t utf16words = e->convert_utf8_to_utf16be(
          source.c_str(), source.size(), utf16_output.get());
      if (utf16words != 0) {
        print_input(source, e);
        abort();
      }
    }
    /**
     * Transcoding from UTF-8 to UTF-32.
     */
    if (validutf8) {
      // We need a buffer of size where to write the UTF-32 words.
      size_t expected_utf32words =
          e->utf32_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char32_t[]> utf32_output{
        new char32_t[expected_utf32words]
      };
      // convert to UTF-32
      size_t utf32words = e->convert_utf8_to_utf32(
          source.c_str(), source.size(), utf32_output.get());
      // It wrote utf32words * sizeof(char32_t) bytes.
      bool validutf32 = e->validate_utf32(utf32_output.get(), utf32words);
      if (!validutf32) {
        return -1;
      }
      // convert it back:
      // We need a buffer of size where to write the UTF-8 words.
      size_t expected_utf8words =
          e->utf8_length_from_utf32(utf32_output.get(), utf32words);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      // convert to UTF-8
      size_t utf8words = e->convert_utf32_to_utf8(
          utf32_output.get(), utf32words, utf8_output.get());
      std::string final_string(utf8_output.get(), utf8words);
      if (source != final_string) {
        print_input(source, e);
        abort();
      }
    } else {
      // invalid input!!!
      size_t expected_utf32words =
          e->utf32_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char32_t[]> utf32_output{
        new char32_t[expected_utf32words]
      };
      // convert to UTF-32
      size_t utf32words = e->convert_utf8_to_utf32(
          source.c_str(), source.size(), utf32_output.get());
      if (utf32words != 0) {
        print_input(source, e);
        abort();
      }
    }

    /**
     * Transcoding from UTF-8 to Latin 1
     */
    if (validutf8) {
      // We need a buffer of size where to write the UTF-16LE words.
      size_t expected_latin1words =
          e->latin1_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to latin1
      size_t latin1words = e->convert_utf8_to_latin1(
          source.c_str(), source.size(), latin1_output.get());
      if(latin1words != 0) {
        // convert it back:
        // We need a buffer of size where to write the UTF-8 words.
        size_t expected_utf8words =
            e->utf8_length_from_latin1(latin1_output.get(), latin1words);
        std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
        // convert to UTF-8
        size_t utf8words = e->convert_latin1_to_utf8(
            latin1_output.get(), latin1words, utf8_output.get());
        std::string final_string(utf8_output.get(), utf8words);
        if (final_string != source) {
          print_input(source, e);
          abort();
        }
      }
    } else {
      // invalid input!!!
      // We need a buffer of size where to write the Latin 1 words.
      size_t expected_latin1words =
          e->latin1_length_from_utf8(source.c_str(), source.size());
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to Latin 1
      size_t latin1words = e->convert_utf8_to_latin1(
          source.c_str(), source.size(), latin1_output.get());
      if (latin1words != 0) {
        print_input(source, e);
        abort();
      }
    }
    /**
     * Transcoding from UTF-16LE to UTF-8.
     */

    // Get new source data here as this will allow the fuzzer to optimize it's input
    // for UTF16-LE.
    source = fdp.ConsumeRandomLengthString(kMaxStringSize);
    bool validutf16le =
        e->validate_utf16le((char16_t *)source.c_str(), source.size() / 2);
    auto rutf16le = e->validate_utf16le_with_errors((char16_t *)source.c_str(), source.size() / 2);
    if(validutf16le != (rutf16le.error == simdutf::SUCCESS)) { // they should agree
      print_input(source, e);
      abort();
    }
    if (validutf16le) {
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf8words = e->utf8_length_from_utf16le(
          (char16_t *)source.c_str(), source.size() / 2);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      size_t utf8words = e->convert_utf16le_to_utf8(
          (char16_t *)source.c_str(), source.size() / 2, utf8_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf8 = e->validate_utf8(utf8_output.get(), utf8words);
      if (!validutf8) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(utf8_output.get(), utf8words);
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-8
      size_t utf16words = e->convert_utf8_to_utf16le(
          utf8_output.get(), utf8words, utf16_output.get());
      for (size_t i = 0; i < source.size() / 2; i++) {
        if (utf16_output.get()[i] != ((char16_t *)source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    } else {
      // invalid input!!!
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf8words = e->utf8_length_from_utf16le(
          (char16_t *)source.c_str(), source.size() / 2);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      size_t utf8words = e->convert_utf16le_to_utf8(
          (char16_t *)source.c_str(), source.size() / 2, utf8_output.get());
      if (utf8words != 0) {
        print_input(source, e);
        abort();
      }
    }

    /**
     * Transcoding from UTF-16BE to UTF-8.
     */
    // Get new source data here as this will allow the fuzzer to optimize it's input
    // for UTF16-BE.
    source = fdp.ConsumeRandomLengthString(kMaxStringSize);
    bool validutf16be =
        e->validate_utf16be((char16_t *)source.c_str(), source.size() / 2);
    auto rutf16be = e->validate_utf16be_with_errors((char16_t *)source.c_str(), source.size() / 2);
    if(validutf16be != (rutf16be.error == simdutf::SUCCESS)) { // they should agree
      print_input(source, e);
      abort();
    }
    if (validutf16be) {
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf8words = e->utf8_length_from_utf16be(
          (char16_t *)source.c_str(), source.size() / 2);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      size_t utf8words = e->convert_utf16be_to_utf8(
          (char16_t *)source.c_str(), source.size() / 2, utf8_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf8 = e->validate_utf8(utf8_output.get(), utf8words);
      if (!validutf8) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf16words =
          e->utf16_length_from_utf8(utf8_output.get(), utf8words);
      std::unique_ptr<char16_t[]> utf16_output{
        new char16_t[expected_utf16words]
      };
      // convert to UTF-8
      size_t utf16words = e->convert_utf8_to_utf16be(
          utf8_output.get(), utf8words, utf16_output.get());
      for (size_t i = 0; i < source.size() / 2; i++) {
        if (utf16_output.get()[i] != ((char16_t *)source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    } else {
      // invalid input!!!
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf8words = e->utf8_length_from_utf16be(
          (char16_t *)source.c_str(), source.size() / 2);
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      size_t utf8words = e->convert_utf16be_to_utf8(
          (char16_t *)source.c_str(), source.size() / 2, utf8_output.get());
      if (utf8words != 0) {
        print_input(source, e);
        abort();
      }
    }


    /**
     * Transcoding from latin1 to UTF-8.
     */
    // Get new source data here as this will allow the fuzzer to optimize it's input
    // for latin1.
    source = fdp.ConsumeRandomLengthString(kMaxStringSize);
    bool validlatin1 = true; // has to be
    if (validlatin1) {
      // We need a buffer of size where to write the UTF-8 words.
      size_t expected_utf8words = e->utf8_length_from_latin1(
          source.c_str(), source.size());
      std::unique_ptr<char[]> utf8_output{ new char[expected_utf8words] };
      size_t utf8words = e->convert_latin1_to_utf8(
          source.c_str(), source.size(), utf8_output.get());
      // It wrote utf8words * sizeof(char) bytes.
      bool validutf8 = e->validate_utf8(utf8_output.get(), utf8words);
      if (!validutf8) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the latin1 words.
      size_t expected_latin1words =
          e->latin1_length_from_utf8(utf8_output.get(), utf8words);
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to latin1
      size_t latin1words = e->convert_utf8_to_latin1(
          utf8_output.get(), utf8words, latin1_output.get());
      for (size_t i = 0; i < source.size(); i++) {
        if (latin1_output.get()[i] != (source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    }
    if (validlatin1) {
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf16words = e->utf16_length_from_latin1(
          source.size());
      std::unique_ptr<char16_t[]> utf16_output{ new char16_t[expected_utf16words] };
      size_t utf16words = e->convert_latin1_to_utf16le(
          source.c_str(), source.size(), utf16_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf16 = e->validate_utf16le(utf16_output.get(), utf16words);
      if (!validutf16) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the latin1 words.
      size_t expected_latin1words =
          e->latin1_length_from_utf16(utf16words);
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to latin1
      size_t latin1words = e->convert_utf16le_to_latin1(
          utf16_output.get(), utf16words, latin1_output.get());
      for (size_t i = 0; i < source.size(); i++) {
        if (latin1_output.get()[i] != (source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    }
    if (validlatin1) {
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf16words = e->utf16_length_from_latin1(
          source.size());
      std::unique_ptr<char16_t[]> utf16_output{ new char16_t[expected_utf16words] };
      size_t utf16words = e->convert_latin1_to_utf16be(
          source.c_str(), source.size(), utf16_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf16 = e->validate_utf16be(utf16_output.get(), utf16words);
      if (!validutf16) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the latin1 words.
      size_t expected_latin1words =
          e->latin1_length_from_utf16(utf16words);
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to latin1
      size_t latin1words = e->convert_utf16be_to_latin1(
          utf16_output.get(), utf16words, latin1_output.get());
      for (size_t i = 0; i < source.size(); i++) {
        if (latin1_output.get()[i] != (source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    }

    if (validlatin1) {
      // We need a buffer of size where to write the UTF-16 words.
      size_t expected_utf32words = e->utf32_length_from_latin1(source.size());
      std::unique_ptr<char32_t[]> utf32_output{ new char32_t[expected_utf32words] };
      size_t utf32words = e->convert_latin1_to_utf32(
          source.c_str(), source.size(), utf32_output.get());
      // It wrote utf16words * sizeof(char16_t) bytes.
      bool validutf32 = e->validate_utf32(utf32_output.get(), utf32words);
      if (!validutf32) {
        print_input(source, e);
        abort();
      }
      // convert it back:
      // We need a buffer of size where to write the latin1 words.
      size_t expected_latin1words =
          e->latin1_length_from_utf32(utf32words);
      std::unique_ptr<char[]> latin1_output{
        new char[expected_latin1words]
      };
      // convert to latin1
      size_t latin1words = e->convert_utf32_to_latin1(
          utf32_output.get(), utf32words, latin1_output.get());
      for (size_t i = 0; i < source.size(); i++) {
        if (latin1_output.get()[i] != (source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
    }

    /// Base64 tests. We begin by trying to decode the input, even if we
    /// expect it to fail.
    {
      size_t max_length_needed = e->maximal_binary_length_from_base64(source.data(), source.size());
      std::vector<char> back(max_length_needed);
      simdutf::result r = e->base64_to_binary(source.data(), source.size(), back.data());
      if (r.error == simdutf::error_code::SUCCESS) {
        // We expect failure but if we succeed, then we should have a roundtrip.
        back.resize(r.count);
        std::vector<char> back2(e->base64_length_from_binary(back.size()));
        size_t base64size = e->binary_to_base64(back.data(), back.size(), back2.data());
        back2.resize(base64size);
        std::vector<char> back3(e->maximal_binary_length_from_base64(
            back2.data(), back2.size()));
        simdutf::result r2 = e->base64_to_binary(back2.data(), back2.size(), back3.data());
        if (r2.error != simdutf::error_code::SUCCESS) {
          print_input(source, e);
          return false;
        }
        if(r2.count != back.size()) {
          print_input(source, e);
          return false;
        }
        if(back3.size() != back.size()) {
          print_input(source, e);
          return false;
        }
      }
    }

    // Same as above, but we use the safe decoder version.
    {
      size_t max_length_needed = e->maximal_binary_length_from_base64(source.data(), source.size());
      std::vector<char> back(max_length_needed);
      simdutf::result r = simdutf::base64_to_binary_safe(source.data(), source.size(), back.data(), max_length_needed);
      if (r.error == simdutf::error_code::SUCCESS) {
        // We expect failure but if we succeed, then we should have a roundtrip.
        back.resize(max_length_needed);
        std::vector<char> back2(e->base64_length_from_binary(back.size()));
        size_t base64size = e->binary_to_base64(back.data(), back.size(), back2.data());
        back2.resize(base64size);
        size_t max_length_needed2 = e->maximal_binary_length_from_base64(
            back2.data(), back2.size());
        std::vector<char> back3(max_length_needed2);
        simdutf::result r2 = simdutf::base64_to_binary_safe(back2.data(), back2.size(), back3.data(), max_length_needed2);
        if (r2.error != simdutf::error_code::SUCCESS) {
          print_input(source, e);
          return false;
        }
        if(max_length_needed != back.size()) {
          print_input(source, e);
          return false;
        }
        if(back3.size() != back.size()) {
          print_input(source, e);
          return false;
        }
      }
    }
    /// Base64 tests. We encode the content as binary in base64 and we decode it,
    /// it should always succeed.
    {
      source = fdp.ConsumeRandomLengthString(kMaxStringSize);
      std::vector<char> base64buffer(e->base64_length_from_binary(source.size()));
      size_t base64size = e->binary_to_base64(source.data(), source.size(), base64buffer.data());
      if(base64size != base64buffer.size()) {
        print_input(source, e);
        abort();
      }
      std::vector<char> back(e->maximal_binary_length_from_base64(
          base64buffer.data(), base64buffer.size()));
      simdutf::result r = e->base64_to_binary(base64buffer.data(), base64buffer.size(), back.data());
      if (r.error != simdutf::error_code::SUCCESS) {
        print_input(source, e);
        abort();
      }
      if(r.count != source.size()) {
        print_input(source, e);
        abort();
      }
      for (size_t i = 0; i < source.size(); i++) {
        if (back[i] != (source.c_str())[i]) {
          print_input(source, e);
          abort();
        }
      }
      size_t max_length = back.size();
      r = simdutf::base64_to_binary_safe(base64buffer.data(), base64buffer.size(), back.data(), max_length);
      if (r.error != simdutf::error_code::SUCCESS) {
        printf("base64 round trip failed, error code %d\n", r.error);
        print_input(source, e);
        return false;
      }
      if(max_length != source.size()) {
        printf("base64 safe round trip failed, not the same size %zu %zu\n", max_length, source.size());
        print_input(source, e);
        return false;
      }
      for (size_t i = 0; i < source.size(); i++) {
        if (back[i] != (source.c_str())[i]) {
          printf("base64 round trip failed, same size, different content\n");
          print_input(source, e);
          return false;
        }
      }
    }

  } // for (auto &e : simdutf::get_available_implementations()) {

  return 0;
} // extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
