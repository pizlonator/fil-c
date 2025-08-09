#include "simdutf/arm64/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_ARM64_H
#error "arm64.h must be included"
#endif
using namespace simd;

simdutf_really_inline bool is_ascii(const simd8x64<uint8_t>& input) {
    simd8<uint8_t> bits = input.reduce_or();
    return bits.max_val() < 0b10000000u;
}

simdutf_unused simdutf_really_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
    simd8<bool> is_second_byte = prev1 >= uint8_t(0b11000000u);
    simd8<bool> is_third_byte  = prev2 >= uint8_t(0b11100000u);
    simd8<bool> is_fourth_byte = prev3 >= uint8_t(0b11110000u);
    // Use ^ instead of | for is_*_byte, because ^ is commutative, and the caller is using ^ as well.
    // This will work fine because we only have to report errors for cases with 0-1 lead bytes.
    // Multiple lead bytes implies 2 overlapping multibyte characters, and if that happens, there is
    // guaranteed to be at least *one* lead byte that is part of only 1 other multibyte character.
    // The error will be detected there.
    return is_second_byte ^ is_third_byte ^ is_fourth_byte;
}

simdutf_really_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
    simd8<bool> is_third_byte  = prev2 >= uint8_t(0b11100000u);
    simd8<bool> is_fourth_byte = prev3 >= uint8_t(0b11110000u);
    return is_third_byte ^ is_fourth_byte;
}

// common functions for utf8 conversions
simdutf_really_inline uint16x4_t convert_utf8_3_byte_to_utf16(uint8x16_t in) {
  // Low half contains  10cccccc|1110aaaa
  // High half contains 10bbbbbb|10bbbbbb
#ifdef SIMDUTF_REGULAR_VISUAL_STUDIO
  const uint8x16_t sh = simdutf_make_uint8x16_t(0, 2, 3, 5, 6, 8, 9, 11, 1, 1, 4, 4, 7, 7, 10, 10);
#else
  const uint8x16_t sh = {0, 2, 3, 5, 6, 8, 9, 11, 1, 1, 4, 4, 7, 7, 10, 10};
#endif
  uint8x16_t perm = vqtbl1q_u8(in, sh);
  // Split into half vectors.
  // 10cccccc|1110aaaa
  uint8x8_t perm_low = vget_low_u8(perm); // no-op
  // 10bbbbbb|10bbbbbb
  uint8x8_t perm_high = vget_high_u8(perm);
  // xxxxxxxx 10bbbbbb
  uint16x4_t mid = vreinterpret_u16_u8(perm_high); // no-op
  // xxxxxxxx 1110aaaa
  uint16x4_t high = vreinterpret_u16_u8(perm_low); // no-op
  // Assemble with shift left insert.
  // xxxxxxaa aabbbbbb
  uint16x4_t mid_high = vsli_n_u16(mid, high, 6);
  // (perm_low << 8) | (perm_low >> 8)
  // xxxxxxxx 10cccccc
  uint16x4_t low = vreinterpret_u16_u8(vrev16_u8(perm_low));
  // Shift left insert into the low bits
  // aaaabbbb bbcccccc
  uint16x4_t composed = vsli_n_u16(low, mid_high, 6);
  return composed;
}

simdutf_really_inline uint16x8_t convert_utf8_2_byte_to_utf16(uint8x16_t in) {
  // Converts 6 2 byte UTF-8 characters to 6 UTF-16 characters.
  // Technically this calculates 8, but 6 does better and happens more often
  // (The languages which use these codepoints use ASCII spaces so 8 would need to be
  // in the middle of a very long word).

  // 10bbbbbb 110aaaaa
  uint16x8_t upper = vreinterpretq_u16_u8(in);
  // (in << 8) | (in >> 8)
  // 110aaaaa 10bbbbbb
  uint16x8_t lower = vreinterpretq_u16_u8(vrev16q_u8(in));
  // 00000000 000aaaaa
  uint16x8_t upper_masked = vandq_u16(upper, vmovq_n_u16(0x1F));
  // Assemble with shift left insert.
  // 00000aaa aabbbbbb
  uint16x8_t composed = vsliq_n_u16(lower, upper_masked, 6);
  return composed;
}

simdutf_really_inline uint16x8_t convert_utf8_1_to_2_byte_to_utf16(uint8x16_t in, size_t shufutf8_idx) {
  // Converts 6 1-2 byte UTF-8 characters to 6 UTF-16 characters.
  // This is a relatively easy scenario
  // we process SIX (6) input code-code units. The max length in bytes of six code
  // code units spanning between 1 and 2 bytes each is 12 bytes.
  uint8x16_t sh = vld1q_u8(reinterpret_cast<const uint8_t*>(simdutf::tables::utf8_to_utf16::shufutf8[shufutf8_idx]));
  // Shuffle
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 110aaaaa 10bbbbbb
  uint16x8_t perm = vreinterpretq_u16_u8(vqtbl1q_u8(in, sh));
  // Mask
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 00000000 00bbbbbb
  uint16x8_t ascii = vandq_u16(perm, vmovq_n_u16(0x7f)); // 6 or 7 bits
  // 1 byte: 00000000 00000000
  // 2 byte: 000aaaaa 00000000
  uint16x8_t highbyte = vandq_u16(perm, vmovq_n_u16(0x1f00)); // 5 bits
  // Combine with a shift right accumulate
  // 1 byte: 00000000 0bbbbbbb
  // 2 byte: 00000aaa aabbbbbb
  uint16x8_t composed = vsraq_n_u16(ascii, highbyte, 2);
  return composed;
}

#include "arm64/arm_validate_utf16.cpp"
#include "arm64/arm_validate_utf32le.cpp"

#include "arm64/arm_convert_latin1_to_utf8.cpp"
#include "arm64/arm_convert_latin1_to_utf16.cpp"
#include "arm64/arm_convert_latin1_to_utf32.cpp"

#include "arm64/arm_convert_utf8_to_utf16.cpp"
#include "arm64/arm_convert_utf8_to_utf32.cpp"
#include "arm64/arm_convert_utf8_to_latin1.cpp"

#include "arm64/arm_convert_utf16_to_latin1.cpp"
#include "arm64/arm_convert_utf16_to_utf8.cpp"
#include "arm64/arm_convert_utf16_to_utf32.cpp"

#include "arm64/arm_convert_utf32_to_latin1.cpp"
#include "arm64/arm_convert_utf32_to_utf8.cpp"
#include "arm64/arm_convert_utf32_to_utf16.cpp"
#include "arm64/arm_base64.cpp"

} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf
#include "generic/buf_block_reader.h"
#include "generic/utf8_validation/utf8_lookup4_algorithm.h"
#include "generic/utf8_validation/utf8_validator.h"
// transcoding from UTF-8 to UTF-16
#include "generic/utf8_to_utf16/valid_utf8_to_utf16.h"
#include "generic/utf8_to_utf16/utf8_to_utf16.h"
// transcoding from UTF-8 to UTF-32
#include "generic/utf8_to_utf32/valid_utf8_to_utf32.h"
#include "generic/utf8_to_utf32/utf8_to_utf32.h"
// other functions
#include "generic/utf8.h"
#include "generic/utf16.h"
// transcoding from UTF-8 to Latin 1
#include "generic/utf8_to_latin1/utf8_to_latin1.h"
#include "generic/utf8_to_latin1/valid_utf8_to_latin1.h"

// placeholder scalars
#include "scalar/latin1.h"

//
// Implementation-specific overrides
//
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {

simdutf_warn_unused int implementation::detect_encodings(const char * input, size_t length) const noexcept {
  // If there is a BOM, then we trust it.
  auto bom_encoding = simdutf::BOM::check_bom(input, length);
  if(bom_encoding != encoding_type::unspecified) { return bom_encoding; }
  // todo: reimplement as a one-pass algorithm.
  int out = 0;
  if(validate_utf8(input, length)) { out |= encoding_type::UTF8; }
  if((length % 2) == 0) {
    if(validate_utf16le(reinterpret_cast<const char16_t*>(input), length/2)) { out |= encoding_type::UTF16_LE; }
  }
  if((length % 4) == 0) {
    if(validate_utf32(reinterpret_cast<const char32_t*>(input), length/4)) { out |= encoding_type::UTF32_LE; }
  }
  return out;
}

simdutf_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return arm64::utf8_validation::generic_validate_utf8(buf,len);
}

simdutf_warn_unused result implementation::validate_utf8_with_errors(const char *buf, size_t len) const noexcept {
  return arm64::utf8_validation::generic_validate_utf8_with_errors(buf,len);
}

simdutf_warn_unused bool implementation::validate_ascii(const char *buf, size_t len) const noexcept {
  return arm64::utf8_validation::generic_validate_ascii(buf,len);
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(const char *buf, size_t len) const noexcept {
  return arm64::utf8_validation::generic_validate_ascii_with_errors(buf,len);
}

simdutf_warn_unused bool implementation::validate_utf16le(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid. protected the implementation from nullptr.
    return true;
  }
  const char16_t* tail = arm_validate_utf16<endianness::LITTLE>(buf, len);
  if (tail) {
    return scalar::utf16::validate<endianness::LITTLE>(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused bool implementation::validate_utf16be(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid. protected the implementation from nullptr.
    return true;
  }
  const char16_t *tail = arm_validate_utf16<endianness::BIG>(buf, len);
  if (tail) {
    return scalar::utf16::validate<endianness::BIG>(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused result implementation::validate_utf16le_with_errors(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return result(error_code::SUCCESS, 0);
  }
  result res = arm_validate_utf16_with_errors<endianness::LITTLE>(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf16::validate_with_errors<endianness::LITTLE>(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused result implementation::validate_utf16be_with_errors(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return result(error_code::SUCCESS, 0);
  }
  result res = arm_validate_utf16_with_errors<endianness::BIG>(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf16::validate_with_errors<endianness::BIG>(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused bool implementation::validate_utf32(const char32_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid. protected the implementation from nullptr.
    return true;
  }
  const char32_t* tail = arm_validate_utf32le(buf, len);
  if (tail) {
    return scalar::utf32::validate(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused result implementation::validate_utf32_with_errors(const char32_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return result(error_code::SUCCESS, 0);
  }
  result res = arm_validate_utf32le_with_errors(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf32::validate_with_errors(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf8(const char * buf, size_t len, char* utf8_output) const noexcept {
  std::pair<const char*, char*> ret = arm_convert_latin1_to_utf8(buf, len, utf8_output);
  size_t converted_chars = ret.second - utf8_output;

  if (ret.first != buf + len) {
    const size_t scalar_converted_chars = scalar::latin1_to_utf8::convert(
      ret.first, len - (ret.first - buf), ret.second);
    converted_chars += scalar_converted_chars;
  }
  return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  std::pair<const char*, char16_t*> ret = arm_convert_latin1_to_utf16<endianness::LITTLE>(buf, len, utf16_output);
  size_t converted_chars = ret.second - utf16_output;
  if (ret.first != buf + len) {
    const size_t scalar_converted_chars = scalar::latin1_to_utf16::convert<endianness::LITTLE>(
      ret.first, len - (ret.first - buf), ret.second);
    converted_chars += scalar_converted_chars;
  }
  return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  std::pair<const char*, char16_t*> ret = arm_convert_latin1_to_utf16<endianness::BIG>(buf, len, utf16_output);
  size_t converted_chars = ret.second - utf16_output;
  if (ret.first != buf + len) {
    const size_t scalar_converted_chars = scalar::latin1_to_utf16::convert<endianness::BIG>(
      ret.first, len - (ret.first - buf), ret.second);
    converted_chars += scalar_converted_chars;
  }
  return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
  std::pair<const char*, char32_t*> ret = arm_convert_latin1_to_utf32(buf, len, utf32_output);
  size_t converted_chars = ret.second - utf32_output;
  if (ret.first != buf + len) {
    const size_t scalar_converted_chars = scalar::latin1_to_utf32::convert(
      ret.first, len - (ret.first - buf), ret.second);
    converted_chars += scalar_converted_chars;
  }
  return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_utf8_to_latin1(const char* buf, size_t len, char* latin1_output) const noexcept {
  utf8_to_latin1::validating_transcoder converter;
  return converter.convert(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_latin1_with_errors(const char* buf, size_t len, char* latin1_output) const noexcept {
  utf8_to_latin1::validating_transcoder converter;
  return converter.convert_with_errors(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_latin1(const char* buf, size_t len, char* latin1_output) const noexcept {
  return arm64::utf8_to_latin1::convert_valid(buf,len,latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  utf8_to_utf16::validating_transcoder converter;
  return converter.convert<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  utf8_to_utf16::validating_transcoder converter;
  return converter.convert<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16le_with_errors(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  utf8_to_utf16::validating_transcoder converter;
  return converter.convert_with_errors<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16be_with_errors(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  utf8_to_utf16::validating_transcoder converter;
  return converter.convert_with_errors<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16le(const char* input, size_t size,
    char16_t* utf16_output) const noexcept {
  return utf8_to_utf16::convert_valid<endianness::LITTLE>(input, size,  utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16be(const char* input, size_t size,
    char16_t* utf16_output) const noexcept {
  return utf8_to_utf16::convert_valid<endianness::BIG>(input, size,  utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf32(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
  utf8_to_utf32::validating_transcoder converter;
  return converter.convert(buf, len, utf32_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf32_with_errors(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
  utf8_to_utf32::validating_transcoder converter;
  return converter.convert_with_errors(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf32(const char* input, size_t size,
    char32_t* utf32_output) const noexcept {
  return utf8_to_utf32::convert_valid(input, size,  utf32_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<const char16_t*, char*> ret = arm_convert_utf16_to_latin1<endianness::LITTLE>(buf, len, latin1_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - latin1_output;

  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_latin1::convert<endianness::LITTLE>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<const char16_t*, char*> ret = arm_convert_utf16_to_latin1<endianness::BIG>(buf, len, latin1_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - latin1_output;

  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_latin1::convert<endianness::BIG>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf16le_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<result, char*> ret = arm_convert_utf16_to_latin1_with_errors<endianness::LITTLE>(buf, len, latin1_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_latin1::convert_with_errors<endianness::LITTLE>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - latin1_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused result implementation::convert_utf16be_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<result, char*> ret = arm_convert_utf16_to_latin1_with_errors<endianness::BIG>(buf, len, latin1_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_latin1::convert_with_errors<endianness::BIG>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - latin1_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  // optimization opportunity: implement a custom function.
  return convert_utf16be_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  // optimization opportunity: implement a custom function.
  return convert_utf16le_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  std::pair<const char16_t*, char*> ret = arm_convert_utf16_to_utf8<endianness::LITTLE>(buf, len, utf8_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf8_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_utf8::convert<endianness::LITTLE>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  std::pair<const char16_t*, char*> ret = arm_convert_utf16_to_utf8<endianness::BIG>(buf, len, utf8_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf8_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_utf8::convert<endianness::BIG>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char*> ret = arm_convert_utf16_to_utf8_with_errors<endianness::LITTLE>(buf, len, utf8_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_utf8::convert_with_errors<endianness::LITTLE>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf8_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char*> ret = arm_convert_utf16_to_utf8_with_errors<endianness::BIG>(buf, len, utf8_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_utf8::convert_with_errors<endianness::BIG>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf8_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return convert_utf16le_to_utf8(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return convert_utf16be_to_utf8(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return 0;
  }
  std::pair<const char32_t*, char*> ret = arm_convert_utf32_to_utf8(buf, len, utf8_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf8_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_utf8::convert(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf8_with_errors(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return result(error_code::SUCCESS, 0);
  }
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char*> ret = arm_convert_utf32_to_utf8_with_errors(buf, len, utf8_output);
  if (ret.first.count != len) {
    result scalar_res = scalar::utf32_to_utf8::convert_with_errors(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf8_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  std::pair<const char16_t*, char32_t*> ret = arm_convert_utf16_to_utf32<endianness::LITTLE>(buf, len, utf32_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf32_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_utf32::convert<endianness::LITTLE>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  std::pair<const char16_t*, char32_t*> ret = arm_convert_utf16_to_utf32<endianness::BIG>(buf, len, utf32_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf32_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf16_to_utf32::convert<endianness::BIG>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf32_with_errors(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char32_t*> ret = arm_convert_utf16_to_utf32_with_errors<endianness::LITTLE>(buf, len, utf32_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_utf32::convert_with_errors<endianness::LITTLE>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf32_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf32_with_errors(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char32_t*> ret = arm_convert_utf16_to_utf32_with_errors<endianness::BIG>(buf, len, utf32_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf16_to_utf32::convert_with_errors<endianness::BIG>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf32_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_utf32_to_latin1(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<const char32_t*, char*> ret = arm_convert_utf32_to_latin1(buf, len, latin1_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - latin1_output;

  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_latin1::convert(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf32_to_latin1_with_errors(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<result, char*> ret = arm_convert_utf32_to_latin1_with_errors(buf, len, latin1_output);
  if (ret.first.error) { return ret.first; }  // Can return directly since scalar fallback already found correct ret.first.count
  if (ret.first.count != len) { // All good so far, but not finished
    result scalar_res = scalar::utf32_to_latin1::convert_with_errors(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - latin1_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_latin1(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<const char32_t*, char*> ret = arm_convert_utf32_to_latin1(buf, len, latin1_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - latin1_output;

  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_latin1::convert_valid(
                                        ret.first, len - (ret.first - buf), ret.second);
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  // optimization opportunity: implement a custom function.
  return convert_utf32_to_utf8(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16le(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  std::pair<const char32_t*, char16_t*> ret = arm_convert_utf32_to_utf16<endianness::LITTLE>(buf, len, utf16_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf16_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_utf16::convert<endianness::LITTLE>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16be(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  std::pair<const char32_t*, char16_t*> ret = arm_convert_utf32_to_utf16<endianness::BIG>(buf, len, utf16_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - utf16_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_utf16::convert<endianness::BIG>(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16le_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char16_t*> ret = arm_convert_utf32_to_utf16_with_errors<endianness::LITTLE>(buf, len, utf16_output);
  if (ret.first.count != len) {
    result scalar_res = scalar::utf32_to_utf16::convert_with_errors<endianness::LITTLE>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf16_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16be_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char16_t*> ret = arm_convert_utf32_to_utf16_with_errors<endianness::BIG>(buf, len, utf16_output);
  if (ret.first.count != len) {
    result scalar_res = scalar::utf32_to_utf16::convert_with_errors<endianness::BIG>(
                                        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count = ret.second - utf16_output;   // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16le(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return convert_utf32_to_utf16le(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16be(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return convert_utf32_to_utf16be(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return convert_utf16le_to_utf32(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return convert_utf16be_to_utf32(buf, len, utf32_output);
}

void implementation::change_endianness_utf16(const char16_t * input, size_t length, char16_t * output) const noexcept {
  utf16::change_endianness_utf16(input, length, output);
}

simdutf_warn_unused size_t implementation::count_utf16le(const char16_t * input, size_t length) const noexcept {
  return utf16::count_code_points<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::count_utf16be(const char16_t * input, size_t length) const noexcept {
  return utf16::count_code_points<endianness::BIG>(input, length);
}

simdutf_warn_unused size_t implementation::count_utf8(const char * input, size_t length) const noexcept {
  return utf8::count_code_points(input, length);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf8(const char* buf, size_t len) const noexcept {
  return count_utf8(buf,len);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf16(size_t length) const noexcept {
  return scalar::utf16::latin1_length_from_utf16(length);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf32(size_t length) const noexcept {
  return scalar::utf32::latin1_length_from_utf32(length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_latin1(const char * input, size_t length) const noexcept {
  // See https://lemire.me/blog/2023/05/15/computing-the-utf-8-size-of-a-latin-1-string-quickly-arm-neon-edition/
  // credit to Pete Cawley
  const uint8_t *data = reinterpret_cast<const uint8_t *>(input);
  uint64_t result = 0;
  const int lanes = sizeof(uint8x16_t);
  uint8_t rem = length % lanes;
  const uint8_t *simd_end = data + (length / lanes) * lanes;
  const uint8x16_t threshold = vdupq_n_u8(0x80);
  for (; data < simd_end; data += lanes) {
    // load 16 bytes
    uint8x16_t input_vec = vld1q_u8(data);
    // compare to threshold (0x80)
    uint8x16_t withhighbit = vcgeq_u8(input_vec, threshold);
    // vertical addition
    result -= vaddvq_s8(vreinterpretq_s8_u8(withhighbit));
  }
  return result + (length / lanes) * lanes + scalar::latin1::utf8_length_from_latin1((const char*)simd_end, rem);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16le(const char16_t * input, size_t length) const noexcept {
  return utf16::utf8_length_from_utf16<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16be(const char16_t * input, size_t length) const noexcept {
  return utf16::utf8_length_from_utf16<endianness::BIG>(input, length);
}


simdutf_warn_unused size_t implementation::utf16_length_from_latin1(size_t length) const noexcept {
  return scalar::latin1::utf16_length_from_latin1(length);
}


simdutf_warn_unused size_t implementation::utf32_length_from_latin1(size_t length) const noexcept {
  return scalar::latin1::utf32_length_from_latin1(length);
}



simdutf_warn_unused size_t implementation::utf32_length_from_utf16le(const char16_t * input, size_t length) const noexcept {
  return utf16::utf32_length_from_utf16<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16be(const char16_t * input, size_t length) const noexcept {
  return utf16::utf32_length_from_utf16<endianness::BIG>(input, length);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf8(const char * input, size_t length) const noexcept {
  return utf8::utf16_length_from_utf8(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  const uint32x4_t v_7f = vmovq_n_u32((uint32_t)0x7f);
  const uint32x4_t v_7ff = vmovq_n_u32((uint32_t)0x7ff);
  const uint32x4_t v_ffff = vmovq_n_u32((uint32_t)0xffff);
  const uint32x4_t v_1 = vmovq_n_u32((uint32_t)0x1);
  size_t pos = 0;
  size_t count = 0;
  for(;pos + 4 <= length; pos += 4) {
    uint32x4_t in = vld1q_u32(reinterpret_cast<const uint32_t *>(input + pos));
    const uint32x4_t ascii_bytes_bytemask = vcleq_u32(in, v_7f);
    const uint32x4_t one_two_bytes_bytemask = vcleq_u32(in, v_7ff);
    const uint32x4_t two_bytes_bytemask = veorq_u32(one_two_bytes_bytemask, ascii_bytes_bytemask);
    const uint32x4_t three_bytes_bytemask = veorq_u32(vcleq_u32(in, v_ffff), one_two_bytes_bytemask);

    const uint16x8_t reduced_ascii_bytes_bytemask = vreinterpretq_u16_u32(vandq_u32(ascii_bytes_bytemask, v_1));
    const uint16x8_t reduced_two_bytes_bytemask = vreinterpretq_u16_u32(vandq_u32(two_bytes_bytemask, v_1));
    const uint16x8_t reduced_three_bytes_bytemask = vreinterpretq_u16_u32(vandq_u32(three_bytes_bytemask, v_1));

    const uint16x8_t compressed_bytemask0 = vpaddq_u16(reduced_ascii_bytes_bytemask, reduced_two_bytes_bytemask);
    const uint16x8_t compressed_bytemask1 = vpaddq_u16(reduced_three_bytes_bytemask, reduced_three_bytes_bytemask);

    size_t ascii_count = count_ones(vgetq_lane_u64(vreinterpretq_u64_u16(compressed_bytemask0), 0));
    size_t two_bytes_count = count_ones(vgetq_lane_u64(vreinterpretq_u64_u16(compressed_bytemask0), 1));
    size_t three_bytes_count = count_ones(vgetq_lane_u64(vreinterpretq_u64_u16(compressed_bytemask1), 0));

    count += 16 - 3*ascii_count - 2*two_bytes_count - three_bytes_count;
  }
  return count + scalar::utf32::utf8_length_from_utf32(input + pos, length - pos);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  const uint32x4_t v_ffff = vmovq_n_u32((uint32_t)0xffff);
  const uint32x4_t v_1 = vmovq_n_u32((uint32_t)0x1);
  size_t pos = 0;
  size_t count = 0;
  for(;pos + 4 <= length; pos += 4) {
    uint32x4_t in = vld1q_u32(reinterpret_cast<const uint32_t *>(input + pos));
    const uint32x4_t surrogate_bytemask = vcgtq_u32(in, v_ffff);
    const uint16x8_t reduced_bytemask = vreinterpretq_u16_u32(vandq_u32(surrogate_bytemask, v_1));
    const uint16x8_t compressed_bytemask = vpaddq_u16(reduced_bytemask, reduced_bytemask);
    size_t surrogate_count = count_ones(vgetq_lane_u64(vreinterpretq_u64_u16(compressed_bytemask), 0));
    count += 4 + surrogate_count;
  }
  return count + scalar::utf32::utf16_length_from_utf32(input + pos, length - pos);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf8(const char * input, size_t length) const noexcept {
  return utf8::count_code_points(input, length);
}

simdutf_warn_unused size_t implementation::maximal_binary_length_from_base64(const char * input, size_t length) const noexcept {
  return scalar::base64::maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result implementation::base64_to_binary(const char * input, size_t length, char* output, base64_options options) const noexcept {
  return (options & base64_url) ? compress_decode_base64<true>(output, input, length, options) : compress_decode_base64<false>(output, input, length, options);
}

simdutf_warn_unused size_t implementation::maximal_binary_length_from_base64(const char16_t * input, size_t length) const noexcept {
  return scalar::base64::maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result implementation::base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options) const noexcept {
  return (options & base64_url) ? compress_decode_base64<true>(output, input, length, options) : compress_decode_base64<false>(output, input, length, options);
}

simdutf_warn_unused size_t implementation::base64_length_from_binary(size_t length, base64_options options) const noexcept {
  return scalar::base64::base64_length_from_binary(length, options);
}

size_t implementation::binary_to_base64(const char * input, size_t length, char* output, base64_options options) const noexcept {
  return encode_base64(output, input, length, options);
}


} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/arm64/end.h"
