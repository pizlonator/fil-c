#include "transcode_test_base.h"
#include "simdutf.h"

#ifndef SIMDUTF_IS_BIG_ENDIAN
#error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif

#include <stdexcept>
#include <algorithm>
#include <string>

#include <tests/reference/encode_utf8.h>
#include <tests/reference/encode_utf16.h>
#include <tests/reference/encode_utf32.h>
#include <tests/reference/encode_latin1.h>
#include <tests/reference/decode_utf16.h>
#include <tests/reference/decode_utf32.h>
#include <tests/reference/validate_utf8.h>
#include <tests/reference/validate_utf16.h>
#include <tests/reference/validate_utf32.h>
#include <tests/reference/validate_utf8_to_latin1.h>
#include <tests/reference/validate_utf16_to_latin1.h>
#include <tests/reference/validate_utf32_to_latin1.h>

namespace simdutf { namespace tests { namespace helpers {

  // C++11 does not have mismatch.
  template<class InputIt1, class InputIt2>
  std::pair<InputIt1, InputIt2> our_mismatch(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2) {
    while (first1 != last1 && first2 != last2 && *first1 == *first2) {
        ++first1, ++first2;
    }
    return std::make_pair(first1, first2);
  }

  void transcode_test_base::encode_utf8(uint32_t codepoint, std::vector<char>& target) {
    ::simdutf::tests::reference::utf8::encode(codepoint, [&target](uint8_t byte) {
      target.push_back(byte);
    });
  }

  void transcode_test_base::encode_utf16(uint32_t codepoint, std::vector<char16_t>& target) {
    char16_t W1;
    char16_t W2;
    switch (::simdutf::tests::reference::utf16::encode(codepoint, W1, W2)) {
      case 1:
#if SIMDUTF_IS_BIG_ENDIAN
        W1 = char16_t((uint16_t(W1)<<8)|(uint16_t(W1)>>8));

#endif
        target.push_back(W1);
        break;

      case 2:
#if SIMDUTF_IS_BIG_ENDIAN
        W1 = char16_t((uint16_t(W1)<<8)|(uint16_t(W1)>>8));
        W2 = char16_t((uint16_t(W2)<<8)|(uint16_t(W2)>>8));
#endif
        target.push_back(W1);
        target.push_back(W2);
        break;

      default:
        throw std::invalid_argument(std::string("Value can't be encoded as UTF-16 code-point : ") + std::to_string(codepoint));
    }
  }

  void transcode_test_base::encode_latin1(uint32_t codepoint, std::vector<char>& target) {
    ::simdutf::tests::reference::latin1::encode(codepoint, [&target](uint8_t byte) {
      target.push_back(byte);
    });
  }

  void transcode_test_base::encode_utf32(uint32_t codepoint, std::vector<char32_t>& target) {
    ::simdutf::tests::reference::utf32::encode(codepoint, [&target](uint32_t word) {
      target.push_back(word);
    });
  }

  /**
   * transcode_latin1_to_utf8_test_base can be used to test Latin1 => UTF8 transcoding.
   */
  transcode_latin1_to_utf8_test_base::transcode_latin1_to_utf8_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_latin1.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }
    output_utf8.resize(reference_output_utf8.size() + output_size_margin);
  }

  void transcode_latin1_to_utf8_test_base::prepare_input(uint32_t codepoint) {
      encode_latin1(codepoint, input_latin1); 
      encode_utf8(codepoint, reference_output_utf8);
  }

  bool transcode_latin1_to_utf8_test_base::is_input_valid() const {
    return true;//
  }

  bool transcode_latin1_to_utf8_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input Latin1 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input Latin1 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }
      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (uint8_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf8.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));
      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      return false;
    }
    // Note that, in general, output_utf8.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf8.begin(), output_utf8.begin() + saved_chars,
                                    reference_output_utf8.begin(), reference_output_utf8.end());
    if (it.first != output_utf8.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf8.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      for(size_t i = 0; i < reference_output_utf8.size(); i++) {
        if(reference_output_utf8[i] != output_utf8[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint8_t(reference_output_utf8[i]), uint8_t(output_utf8[i]));
      }
      return false;
    }

    return true;
  }

  /**
   * transcode_latin1_to_utf16_test_base can be used to test Latin1 => utf16 transcoding.
   */
  transcode_latin1_to_utf16_test_base::transcode_latin1_to_utf16_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_latin1.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf16.resize(reference_output_utf16.size() + output_size_margin);
  }

  void transcode_latin1_to_utf16_test_base::prepare_input(uint32_t codepoint) {
      encode_latin1(codepoint, input_latin1); 

      encode_utf16(codepoint, reference_output_utf16);
  }

  bool transcode_latin1_to_utf16_test_base::is_input_valid() const {
    return true;
  }

  bool transcode_latin1_to_utf16_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input Latin1 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input Latin1 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char16_t>& array) {
    printf("%s", title);
    for (size_t i=0; i < saved_chars; i++) {
      printf(" %04x", (uint16_t)array[i]); // Use %04x to print 16-bit hexadecimal numbers
    }
    putchar('\n');
    };

    if (saved_chars != reference_output_utf16.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));
      dump("expected :", reference_output_utf16);
      dump("actual   :", output_utf16);
      return false;
    }
    // Note that, in general, output_utf16.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf16.begin(), output_utf16.begin() + saved_chars,
                                    reference_output_utf16.begin(), reference_output_utf16.end());
    if (it.first != output_utf16.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf16.begin(), it.first)), uint16_t(*it.first), uint16_t(*it.second));
      dump("expected :", reference_output_utf16);
      dump("actual   :", output_utf16);
      for(size_t i = 0; i < reference_output_utf16.size(); i++) {
        if(reference_output_utf16[i] != output_utf16[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint16_t(reference_output_utf16[i]), uint16_t(output_utf16[i]));
      }
      return false;
    }
    return true;
  }

  /**
   * transcode_latin1_to_utf32_test_base can be used to test UTF-32 => UTF-16LE transcoding.
   */
  transcode_latin1_to_utf32_test_base::transcode_latin1_to_utf32_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_latin1.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }
    output_utf32.resize(reference_output_utf32.size() + output_size_margin);
  }

  void transcode_latin1_to_utf32_test_base::prepare_input(uint32_t codepoint) {
      encode_latin1(codepoint, input_latin1); 
      encode_utf32(codepoint, reference_output_utf32);
  }

  bool transcode_latin1_to_utf32_test_base::is_input_valid() const {
    return true;
  }

  bool transcode_latin1_to_utf32_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input Latin1 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input Latin1 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }
      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char32_t>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (unsigned int)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf32.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));
      dump("expected :", reference_output_utf32);
      dump("actual   :", output_utf32);
      return false;
    }
    // Note that, in general, output_utf32.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf32.begin(), output_utf32.begin() + saved_chars,
                                    reference_output_utf32.begin(), reference_output_utf32.end());
    if (it.first != output_utf32.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf32.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_utf32);
      dump("actual   :", output_utf32);
      for(size_t i = 0; i < reference_output_utf32.size(); i++) {
        if(reference_output_utf32[i] != output_utf32[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint8_t(reference_output_utf32[i]), uint8_t(output_utf32[i]));
      }
      return false;
    }
    return true;
  }

  /**
   * transcode_utf8_to_latin1_test_base can be used to test UTF-32 => UTF-16LE transcoding.
   */
  transcode_utf8_to_latin1_test_base::transcode_utf8_to_latin1_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf8.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }
    output_latin1.resize(reference_output_latin1.size() + output_size_margin);
  }

  void transcode_utf8_to_latin1_test_base::prepare_input(uint32_t codepoint) {
      encode_utf8(codepoint, input_utf8);
      encode_latin1(codepoint, reference_output_latin1);
  }

  bool transcode_utf8_to_latin1_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf8_to_latin1(input_utf8.data(), input_utf8.size());
  }

  bool transcode_utf8_to_latin1_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-8 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-8 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (char)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_latin1.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      return false;
    }
    // Note that, in general, output_latin1.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_latin1.begin(), output_latin1.begin() + saved_chars,
                                    reference_output_latin1.begin(), reference_output_latin1.end());
    if (it.first != output_latin1.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_latin1.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      for(size_t i = 0; i < reference_output_latin1.size(); i++) {
        if(reference_output_latin1[i] != output_latin1[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint8_t(reference_output_latin1[i]), uint8_t(output_latin1[i]));
      }
      return false;
    }

    return true;
  }

  /**
   * transcode_utf16_to_latin1_test_base can be used to test UTF-16 => Latin1 transcoding.
   */
  transcode_utf16_to_latin1_test_base::transcode_utf16_to_latin1_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf16.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }
    output_latin1.resize(reference_output_latin1.size() + output_size_margin);
  }

  transcode_utf16_to_latin1_test_base::transcode_utf16_to_latin1_test_base(const std::vector<char16_t>& input_utf16)
    : input_utf16{input_utf16} {
    auto consume = [this](const uint32_t codepoint) {
      ::simdutf::tests::reference::latin1::encode(codepoint, [this](uint32_t byte) {
        reference_output_latin1.push_back(byte);
      });
    };

    auto error_handler = [](const char16_t*, const char16_t*,  simdutf::tests::reference::utf16::Error) -> bool {
      throw std::invalid_argument("Wrong UTF-16 input");
    };
    simdutf::tests::reference::utf16::decode(input_utf16.data(), input_utf16.size(), consume, error_handler);
    output_latin1.resize(reference_output_latin1.size() + output_size_margin);
  }


  void transcode_utf16_to_latin1_test_base::prepare_input(uint32_t codepoint) {
      encode_utf16(codepoint, input_utf16);
      encode_latin1(codepoint, reference_output_latin1);
  }

  bool transcode_utf16_to_latin1_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf16_to_latin1(input_utf16.data(), input_utf16.size());
  }

  bool transcode_utf16_to_latin1_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-16 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-16 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %08x", (uint32_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_latin1.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      return false;
    }
    // Note that, in general, output_latin1.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_latin1.begin(), output_latin1.begin() + saved_chars,
                                    reference_output_latin1.begin(), reference_output_latin1.end());
    if (it.first != output_latin1.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_latin1.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      for(size_t i = 0; i < reference_output_latin1.size(); i++) {
        if(reference_output_latin1[i] != output_latin1[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%08x and got 0x%08x\n ", i, uint8_t(reference_output_latin1[i]), uint8_t(output_latin1[i]));
      }
      return false;
    }
    return true;
  }

  /**
   * transcode_utf8_to_utf16_test_base can be used to test UTF-8 => UTF-16 transcoding.
   */
  transcode_utf8_to_utf16_test_base::transcode_utf8_to_utf16_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf8.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf16.resize(reference_output_utf16.size() + output_size_margin);
  }

  void transcode_utf8_to_utf16_test_base::prepare_input(uint32_t codepoint) {
      encode_utf8(codepoint, input_utf8);
      encode_utf16(codepoint, reference_output_utf16);
  }

  bool transcode_utf8_to_utf16_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf8(input_utf8.data(), input_utf8.size());
  }

  bool transcode_utf8_to_utf16_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-8 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-8 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }
    if (saved_chars != reference_output_utf16.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));
      return false;
    }

    // Note that, in general, output_utf16.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf16.begin(), output_utf16.begin() + saved_chars,
                                    reference_output_utf16.begin(), reference_output_utf16.end());
    if (it.first != output_utf16.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%04x, expected 0x%04x\n",
             size_t(std::distance(output_utf16.begin(), it.first)), uint16_t(*it.first), uint16_t(*it.second));
      for(size_t i = 0; i < output_utf16.size(); i++) {
        if(reference_output_utf16[i] != output_utf16[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x and got 0x%04x\n ", i, uint16_t(reference_output_utf16[i]), uint16_t(output_utf16[i]));
      }

      return false;
    }

    return true;
  }

  /**
   * transcode_utf8_to_utf32_test_base can be used to test UTF-8 => UTF-32 transcoding.
   */
  transcode_utf8_to_utf32_test_base::transcode_utf8_to_utf32_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf8.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf32.resize(reference_output_utf32.size() + output_size_margin);
  }

  void transcode_utf8_to_utf32_test_base::prepare_input(uint32_t codepoint) {
      encode_utf8(codepoint, input_utf8);
      encode_utf32(codepoint, reference_output_utf32);
  }

  bool transcode_utf8_to_utf32_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf8(input_utf8.data(), input_utf8.size());
  }

  bool transcode_utf8_to_utf32_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-8 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-8 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    if (saved_chars != reference_output_utf32.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));
      return false;
    }

    // Note that, in general, output_utf16.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf32.begin(), output_utf32.begin() + saved_chars,
                                    reference_output_utf32.begin(), reference_output_utf32.end());
    if (it.first != output_utf32.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%04x, expected 0x%04x\n",
             size_t(std::distance(output_utf32.begin(), it.first)), uint16_t(*it.first), uint16_t(*it.second));
      for(size_t i = 0; i < output_utf32.size(); i++) {
        if(reference_output_utf32[i] != output_utf32[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%08x and got 0x%08x\n ", i, uint32_t(reference_output_utf32[i]), uint32_t(output_utf32[i]));
      }

      return false;
    }

    return true;
  }

  /**
   * transcode_utf16_to_utf8_test_base can be used to test UTF-16 => UTF-8 transcoding.
   */
  transcode_utf16_to_utf8_test_base::transcode_utf16_to_utf8_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf16.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf8.resize(reference_output_utf8.size() + output_size_margin);
  }

  transcode_utf16_to_utf8_test_base::transcode_utf16_to_utf8_test_base(const std::vector<char16_t>& input_utf16)
    : input_utf16{input_utf16} {

    auto consume = [this](const uint32_t codepoint) {
      ::simdutf::tests::reference::utf8::encode(codepoint, [this](uint8_t byte) {
        reference_output_utf8.push_back(byte);
      });
    };

    auto error_handler = [](const char16_t*, const char16_t*,  simdutf::tests::reference::utf16::Error) -> bool {
      throw std::invalid_argument("Wrong UTF-16 input");
    };
    simdutf::tests::reference::utf16::decode(input_utf16.data(), input_utf16.size(), consume, error_handler);
    output_utf8.resize(reference_output_utf8.size() + output_size_margin);
  }

  void transcode_utf16_to_utf8_test_base::prepare_input(uint32_t codepoint) {
      encode_utf16(codepoint, input_utf16);
      encode_utf8(codepoint, reference_output_utf8);
  }

  bool transcode_utf16_to_utf8_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf16(input_utf16.data(), input_utf16.size());
  }

  bool transcode_utf16_to_utf8_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-16 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-16 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (uint8_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf8.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));

      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      return false;
    }
    // Note that, in general, output_utf8.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf8.begin(), output_utf8.begin() + saved_chars,
                                    reference_output_utf8.begin(), reference_output_utf8.end());
    if (it.first != output_utf8.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf8.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      for(size_t i = 0; i < reference_output_utf8.size(); i++) {
        if(reference_output_utf8[i] != output_utf8[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%02x and got 0x%02x\n ", i, uint8_t(reference_output_utf8[i]), uint8_t(output_utf8[i]));
      }
      return false;
    }

    return true;
  }


  /**
   * transcode_utf16_to_utf32_test_base can be used to test UTF-16LE => UTF-32 transcoding.
   */
  transcode_utf16_to_utf32_test_base::transcode_utf16_to_utf32_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf16.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf32.resize(reference_output_utf32.size() + output_size_margin);
  }

  transcode_utf16_to_utf32_test_base::transcode_utf16_to_utf32_test_base(const std::vector<char16_t>& input_utf16)
    : input_utf16{input_utf16} {

    auto consume = [this](const uint32_t codepoint) {
      ::simdutf::tests::reference::utf32::encode(codepoint, [this](uint32_t byte) {
        reference_output_utf32.push_back(byte);
      });
    };

    auto error_handler = [](const char16_t*, const char16_t*,  simdutf::tests::reference::utf16::Error) -> bool {
      throw std::invalid_argument("Wrong UTF-16 input");
    };
    simdutf::tests::reference::utf16::decode(input_utf16.data(), input_utf16.size(), consume, error_handler);
    output_utf32.resize(reference_output_utf32.size() + output_size_margin);
  }


  void transcode_utf16_to_utf32_test_base::prepare_input(uint32_t codepoint) {
      encode_utf16(codepoint, input_utf16);
      encode_utf32(codepoint, reference_output_utf32);
  }

  bool transcode_utf16_to_utf32_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf16(input_utf16.data(), input_utf16.size());
  }

  bool transcode_utf16_to_utf32_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-16 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-16 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char32_t>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %08x", (uint32_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf32.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf32.size()));

      dump("expected :", reference_output_utf32);
      dump("actual   :", output_utf32);
      return false;
    }
    // Note that, in general, output_utf32.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf32.begin(), output_utf32.begin() + saved_chars,
                                    reference_output_utf32.begin(), reference_output_utf32.end());
    if (it.first != output_utf32.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf32.begin(), it.first)), uint32_t(*it.first), uint32_t(*it.second));

      dump("expected :", reference_output_utf32);
      dump("actual   :", output_utf32);
      for(size_t i = 0; i < reference_output_utf32.size(); i++) {
        if(reference_output_utf32[i] != output_utf32[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%08x and got 0x%08x\n ", i, uint32_t(reference_output_utf32[i]), uint32_t(output_utf32[i]));
      }
      return false;
    }

    return true;
  }

  /**
   * transcode_utf32_to_latin1_test_base can be used to test UTF-32 => UTF-16LE transcoding.
   */
  transcode_utf32_to_latin1_test_base::transcode_utf32_to_latin1_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf32.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }
    output_latin1.resize(reference_output_latin1.size() + output_size_margin);
  }

  void transcode_utf32_to_latin1_test_base::prepare_input(uint32_t codepoint) {
      encode_utf32(codepoint, input_utf32);
      encode_latin1(codepoint, reference_output_latin1); //-- not applicable? All of a byte is translatable to latin1
  }

  bool transcode_utf32_to_latin1_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf32_to_latin1(input_utf32.data(), input_utf32.size());
  }

  bool transcode_utf32_to_latin1_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-32 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-32 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }
      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (char)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_latin1.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_latin1.size()));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      return false;
    }
    // Note that, in general, output_latin1.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_latin1.begin(), output_latin1.begin() + saved_chars,
                                    reference_output_latin1.begin(), reference_output_latin1.end());
    if (it.first != output_latin1.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_latin1.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_latin1);
      dump("actual   :", output_latin1);
      for(size_t i = 0; i < reference_output_latin1.size(); i++) {
        if(reference_output_latin1[i] != output_latin1[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint8_t(reference_output_latin1[i]), uint8_t(output_latin1[i]));
      }
      return false;
    }
    return true;
  }

  /**
   * transcode_utf32_to_utf8_test_base can be used to test UTF-32 => UTF-8 transcoding.
   */
  transcode_utf32_to_utf8_test_base::transcode_utf32_to_utf8_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf32.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf8.resize(reference_output_utf8.size() + output_size_margin);
  }

  transcode_utf32_to_utf8_test_base::transcode_utf32_to_utf8_test_base(const std::vector<char32_t>& input_utf32)
    : input_utf32{input_utf32} {

    auto consume = [this](const uint32_t codepoint) {
      ::simdutf::tests::reference::utf8::encode(codepoint, [this](uint8_t byte) {
        reference_output_utf8.push_back(byte);
      });
    };

    auto error_handler = [](const char32_t*, const char32_t*,  simdutf::tests::reference::utf32::Error) -> bool {
      throw std::invalid_argument("Wrong UTF-32 input");
    };
    simdutf::tests::reference::utf32::decode(input_utf32.data(), input_utf32.size(), consume, error_handler);
    output_utf8.resize(reference_output_utf8.size() + output_size_margin);
  }

  void transcode_utf32_to_utf8_test_base::prepare_input(uint32_t codepoint) {
      encode_utf32(codepoint, input_utf32);
      encode_utf8(codepoint, reference_output_utf8);
  }

  bool transcode_utf32_to_utf8_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf32(input_utf32.data(), input_utf32.size());
  }

  bool transcode_utf32_to_utf8_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-32 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-32 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (uint8_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf8.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf8.size()));

      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      return false;
    }
    // Note that, in general, output_utf8.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf8.begin(), output_utf8.begin() + saved_chars,
                                    reference_output_utf8.begin(), reference_output_utf8.end());
    if (it.first != output_utf8.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf8.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_utf8);
      dump("actual   :", output_utf8);
      for(size_t i = 0; i < reference_output_utf8.size(); i++) {
        if(reference_output_utf8[i] != output_utf8[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%02x and got 0x%02x\n ", i, uint8_t(reference_output_utf8[i]), uint8_t(output_utf8[i]));
      }
      return false;
    }

    return true;
  }


  /**
   * transcode_utf32_to_utf16_test_base can be used to test UTF-32 => UTF-16LE transcoding.
   */
  transcode_utf32_to_utf16_test_base::transcode_utf32_to_utf16_test_base(GenerateCodepoint generate,
                                       size_t input_size) {
    while (input_utf32.size() < input_size) {
      const uint32_t codepoint = generate();
      prepare_input(codepoint);
    }

    output_utf16.resize(reference_output_utf16.size() + output_size_margin);
  }

  void transcode_utf32_to_utf16_test_base::prepare_input(uint32_t codepoint) {
      encode_utf32(codepoint, input_utf32);
      encode_utf16(codepoint, reference_output_utf16);
  }

  bool transcode_utf32_to_utf16_test_base::is_input_valid() const {
    return simdutf::tests::reference::validate_utf32(input_utf32.data(), input_utf32.size());
  }

  bool transcode_utf32_to_utf16_test_base::validate(size_t saved_chars) const {
    if (!is_input_valid()) {
      if (saved_chars != 0) {
        printf("input UTF-32 string is not valid, but conversion routine returned %zu, indicating a valid input\n", saved_chars);
        return false;
      }
    }
    if (saved_chars == 0) {
      if (is_input_valid()) {
        printf("input UTF-32 string is valid, but conversion routine returned 0, indicating input error");
        return false;
      }

      return true;
    }

    auto dump = [saved_chars](const char* title, const std::vector<char16_t>& array) {
      printf("%s", title);
      for (size_t i=0; i < saved_chars; i++) {
        printf(" %02x", (uint16_t)array[i]);
      }
      putchar('\n');
    };

    if (saved_chars != reference_output_utf16.size()) {
      printf("wrong saved bytes value: procedure returned %zu bytes, it should be %zu\n",
             size_t(saved_chars), size_t(reference_output_utf16.size()));

      dump("expected :", reference_output_utf16);
      dump("actual   :", output_utf16);
      return false;
    }
    // Note that, in general, output_utf16.size() will not matched saved_chars.

    // At this point, we know that the lengths are the same so std::mismatch is enough
    // to tell us whether the strings are identical.
    auto it = our_mismatch(output_utf16.begin(), output_utf16.begin() + saved_chars,
                                    reference_output_utf16.begin(), reference_output_utf16.end());
    if (it.first != output_utf16.begin() + saved_chars) {
      printf("mismatched output at %zu: actual value 0x%02x, expected 0x%02x\n",
             size_t(std::distance(output_utf16.begin(), it.first)), uint8_t(*it.first), uint8_t(*it.second));

      dump("expected :", reference_output_utf16);
      dump("actual   :", output_utf16);
      for(size_t i = 0; i < reference_output_utf16.size(); i++) {
        if(reference_output_utf16[i] != output_utf16[i]) { printf(" ==> "); }
        printf("at %zu expected 0x%04x 0x%04x\n ", i, uint16_t(reference_output_utf16[i]), uint16_t(output_utf16[i]));
      }
      return false;
    }

    return true;
  }




}}}
