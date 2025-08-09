#include "simdutf/westmere/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_WESTMERE_H
#error "westmere.h must be included"
#endif
using namespace simd;

simdutf_really_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

simdutf_unused simdutf_really_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_second_byte = prev1.saturating_sub(0b11000000u-1); // Only 11______ will be > 0
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0b11100000u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0b11110000u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_second_byte | is_third_byte | is_fourth_byte) > int8_t(0);
}

simdutf_really_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-0x80); // Only 111_____ will be >= 0x80
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-0x80); // Only 1111____ will be >= 0x80
  return simd8<bool>(is_third_byte | is_fourth_byte);
}

#include "westmere/internal/loader.cpp"

#include "westmere/sse_validate_utf16.cpp"
#include "westmere/sse_validate_utf32le.cpp"

#include "westmere/sse_convert_latin1_to_utf8.cpp"
#include "westmere/sse_convert_latin1_to_utf16.cpp"
#include "westmere/sse_convert_latin1_to_utf32.cpp"


#include "westmere/sse_convert_utf8_to_utf16.cpp"
#include "westmere/sse_convert_utf8_to_utf32.cpp"
#include "westmere/sse_convert_utf8_to_latin1.cpp"

#include "westmere/sse_convert_utf16_to_latin1.cpp"
#include "westmere/sse_convert_utf16_to_utf8.cpp"
#include "westmere/sse_convert_utf16_to_utf32.cpp"

#include "westmere/sse_convert_utf32_to_latin1.cpp"
#include "westmere/sse_convert_utf32_to_utf8.cpp"
#include "westmere/sse_convert_utf32_to_utf16.cpp"
#include "westmere/sse_base64.cpp"

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


//
// Implementation-specific overrides
//

namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {

simdutf_warn_unused int implementation::detect_encodings(const char * input, size_t length) const noexcept {
  // If there is a BOM, then we trust it.
  auto bom_encoding = simdutf::BOM::check_bom(input, length);
  // todo: reimplement as a one-pass algorithm.
  if(bom_encoding != encoding_type::unspecified) { return bom_encoding; }
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
  return westmere::utf8_validation::generic_validate_utf8(buf, len);
}

simdutf_warn_unused result implementation::validate_utf8_with_errors(const char *buf, size_t len) const noexcept {
  return westmere::utf8_validation::generic_validate_utf8_with_errors(buf, len);
}

simdutf_warn_unused bool implementation::validate_ascii(const char *buf, size_t len) const noexcept {
  return westmere::utf8_validation::generic_validate_ascii(buf, len);
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(const char *buf, size_t len) const noexcept {
  return westmere::utf8_validation::generic_validate_ascii_with_errors(buf,len);
}

simdutf_warn_unused bool implementation::validate_utf16le(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid UTF-16. protect the implementation from
    // handling nullptr
    return true;
  }
  const char16_t *tail = sse_validate_utf16<endianness::LITTLE>(buf, len);
  if (tail) {
    return scalar::utf16::validate<endianness::LITTLE>(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused bool implementation::validate_utf16be(const char16_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid UTF-16. protect the implementation from
    // handling nullptr
    return true;
  }
  const char16_t *tail = sse_validate_utf16<endianness::BIG>(buf, len);
  if (tail) {
    return scalar::utf16::validate<endianness::BIG>(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused result implementation::validate_utf16le_with_errors(const char16_t *buf, size_t len) const noexcept {
  result res = sse_validate_utf16_with_errors<endianness::LITTLE>(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf16::validate_with_errors<endianness::LITTLE>(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused result implementation::validate_utf16be_with_errors(const char16_t *buf, size_t len) const noexcept {
  result res = sse_validate_utf16_with_errors<endianness::BIG>(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf16::validate_with_errors<endianness::BIG>(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused bool implementation::validate_utf32(const char32_t *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    // empty input is valid UTF-32. protect the implementation from
    // handling nullptr
    return true;
  }
  const char32_t *tail = sse_validate_utf32le(buf, len);
  if (tail) {
    return scalar::utf32::validate(tail, len - (tail - buf));
  } else {
    return false;
  }
}

simdutf_warn_unused result implementation::validate_utf32_with_errors(const char32_t *buf, size_t len) const noexcept {
  if (len == 0) {
    // empty input is valid UTF-32. protect the implementation from
    // handling nullptr
    return result(error_code::SUCCESS, 0);
  }
  result res = sse_validate_utf32le_with_errors(buf, len);
  if (res.count != len) {
    result scalar_res = scalar::utf32::validate_with_errors(buf + res.count, len - res.count);
    return result(scalar_res.error, res.count + scalar_res.count);
  } else {
    return res;
  }
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf8(const char * buf, size_t len, char* utf8_output) const noexcept {

  std::pair<const char*, char*> ret = sse_convert_latin1_to_utf8(buf, len, utf8_output);
  size_t converted_chars = ret.second - utf8_output;

  if (ret.first != buf + len) {
    const size_t scalar_converted_chars = scalar::latin1_to_utf8::convert(
      ret.first, len - (ret.first - buf), ret.second);
    converted_chars += scalar_converted_chars;
  }

  return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
    std::pair<const char*, char16_t*> ret = sse_convert_latin1_to_utf16<endianness::LITTLE>(buf, len, utf16_output);
    if (ret.first == nullptr) { return 0; }
    size_t converted_chars = ret.second - utf16_output;
    if (ret.first != buf + len) {
        const size_t scalar_converted_chars = scalar::latin1_to_utf16::convert<endianness::LITTLE>(
                                              ret.first, len - (ret.first - buf), ret.second);
        if (scalar_converted_chars == 0) { return 0; }
        converted_chars += scalar_converted_chars;
    }
    return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
    std::pair<const char*, char16_t*> ret = sse_convert_latin1_to_utf16<endianness::BIG>(buf, len, utf16_output);
    if (ret.first == nullptr) { return 0; }
    size_t converted_chars = ret.second - utf16_output;
    if (ret.first != buf + len) {
        const size_t scalar_converted_chars = scalar::latin1_to_utf16::convert<endianness::BIG>(
                                              ret.first, len - (ret.first - buf), ret.second);
        if (scalar_converted_chars == 0) { return 0; }
        converted_chars += scalar_converted_chars;
    }
    return converted_chars;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
    std::pair<const char*, char32_t*> ret = sse_convert_latin1_to_utf32(buf, len, utf32_output);
    if (ret.first == nullptr) { return 0; }
    size_t converted_chars = ret.second - utf32_output;
    if (ret.first != buf + len) {
        const size_t scalar_converted_chars = scalar::latin1_to_utf32::convert(
                                              ret.first, len - (ret.first - buf), ret.second);
        if (scalar_converted_chars == 0) { return 0; }
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
  return westmere::utf8_to_latin1::convert_valid(buf,len,latin1_output);
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
  std::pair<const char16_t*, char*> ret = sse_convert_utf16_to_latin1<endianness::LITTLE>(buf, len, latin1_output);
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
  std::pair<const char16_t*, char*> ret = sse_convert_utf16_to_latin1<endianness::BIG>(buf, len, latin1_output);
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
  std::pair<result, char*> ret = sse_convert_utf16_to_latin1_with_errors<endianness::LITTLE>(buf, len, latin1_output);
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
  std::pair<result, char*> ret = sse_convert_utf16_to_latin1_with_errors<endianness::BIG>(buf, len, latin1_output);
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
  // optimization opportunity: we could provide an optimized function.
  return convert_utf16be_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  // optimization opportunity: we could provide an optimized function.
  return convert_utf16le_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  std::pair<const char16_t*, char*> ret = sse_convert_utf16_to_utf8<endianness::LITTLE>(buf, len, utf8_output);
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
  std::pair<const char16_t*, char*> ret = sse_convert_utf16_to_utf8<endianness::BIG>(buf, len, utf8_output);
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
  std::pair<result, char*> ret = westmere::sse_convert_utf16_to_utf8_with_errors<endianness::LITTLE>(buf, len, utf8_output);
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
  std::pair<result, char*> ret = westmere::sse_convert_utf16_to_utf8_with_errors<endianness::BIG>(buf, len, utf8_output);
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

simdutf_warn_unused size_t implementation::convert_utf32_to_latin1(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  std::pair<const char32_t*, char*> ret = sse_convert_utf32_to_latin1(buf, len, latin1_output);
  if (ret.first == nullptr) { return 0; }
  size_t saved_bytes = ret.second - latin1_output;
  // if (ret.first != buf + len) {
  if (ret.first < buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_latin1::convert(
                                        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) { return 0; }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}


simdutf_warn_unused result implementation::convert_utf32_to_latin1_with_errors(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char*> ret = westmere::sse_convert_utf32_to_latin1_with_errors(buf, len, latin1_output);
  if (ret.first.count != len) {
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
  // optimization opportunity: we could provide an optimized function.
  return convert_utf32_to_latin1(buf,len,latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  std::pair<const char32_t*, char*> ret = sse_convert_utf32_to_utf8(buf, len, utf8_output);
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
  // ret.first.count is always the position in the buffer, not the number of code units written even if finished
  std::pair<result, char*> ret = westmere::sse_convert_utf32_to_utf8_with_errors(buf, len, utf8_output);
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
  std::pair<const char16_t*, char32_t*> ret = sse_convert_utf16_to_utf32<endianness::LITTLE>(buf, len, utf32_output);
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
  std::pair<const char16_t*, char32_t*> ret = sse_convert_utf16_to_utf32<endianness::BIG>(buf, len, utf32_output);
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
  std::pair<result, char32_t*> ret = westmere::sse_convert_utf16_to_utf32_with_errors<endianness::LITTLE>(buf, len, utf32_output);
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
  std::pair<result, char32_t*> ret = westmere::sse_convert_utf16_to_utf32_with_errors<endianness::BIG>(buf, len, utf32_output);
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

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  return convert_utf32_to_utf8(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16le(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  std::pair<const char32_t*, char16_t*> ret = sse_convert_utf32_to_utf16<endianness::LITTLE>(buf, len, utf16_output);
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
  std::pair<const char32_t*, char16_t*> ret = sse_convert_utf32_to_utf16<endianness::BIG>(buf, len, utf16_output);
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
  std::pair<result, char16_t*> ret = westmere::sse_convert_utf32_to_utf16_with_errors<endianness::LITTLE>(buf, len, utf16_output);
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
  std::pair<result, char16_t*> ret = westmere::sse_convert_utf32_to_utf16_with_errors<endianness::BIG>(buf, len, utf16_output);
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

simdutf_warn_unused size_t implementation::utf8_length_from_latin1(const char * input, size_t len) const noexcept {
  const uint8_t *str = reinterpret_cast<const uint8_t *>(input);
  size_t answer = len / sizeof(__m128i) * sizeof(__m128i);
  size_t i = 0;
  if(answer >= 2048) { // long strings optimization
    __m128i two_64bits = _mm_setzero_si128();
    while (i + sizeof(__m128i) <= len) {
      __m128i runner = _mm_setzero_si128();
      size_t iterations = (len - i) / sizeof(__m128i);
      if (iterations > 255) {
        iterations = 255;
      }
      size_t max_i = i + iterations * sizeof(__m128i) - sizeof(__m128i);
      for (; i + 4*sizeof(__m128i) <= max_i; i += 4*sizeof(__m128i)) {
        __m128i input1 = _mm_loadu_si128((const __m128i *)(str + i));
        __m128i input2 = _mm_loadu_si128((const __m128i *)(str + i + sizeof(__m128i)));
        __m128i input3 = _mm_loadu_si128((const __m128i *)(str + i + 2*sizeof(__m128i)));
        __m128i input4 = _mm_loadu_si128((const __m128i *)(str + i + 3*sizeof(__m128i)));
        __m128i input12 = _mm_add_epi8(
                                        _mm_cmpgt_epi8(
                                                      _mm_setzero_si128(),
                                                      input1),
                                        _mm_cmpgt_epi8(
                                                      _mm_setzero_si128(),
                                                      input2));
        __m128i input34 = _mm_add_epi8(
                                        _mm_cmpgt_epi8(
                                                      _mm_setzero_si128(),
                                                      input3),
                                        _mm_cmpgt_epi8(
                                                      _mm_setzero_si128(),
                                                      input4));
        __m128i input1234 = _mm_add_epi8(input12, input34);
        runner = _mm_sub_epi8(runner, input1234);
      }
      for (; i <= max_i; i += sizeof(__m128i)) {
        __m128i more_input = _mm_loadu_si128((const __m128i *)(str + i));
        runner = _mm_sub_epi8(
            runner, _mm_cmpgt_epi8(_mm_setzero_si128(), more_input));
      }
      two_64bits = _mm_add_epi64(
          two_64bits, _mm_sad_epu8(runner, _mm_setzero_si128()));
    }
    answer += _mm_extract_epi64(two_64bits, 0) +
              _mm_extract_epi64(two_64bits, 1);
  } else if (answer > 0) { // short string optimization
    for(; i + 2*sizeof(__m128i) <= len; i += 2*sizeof(__m128i)) {
      __m128i latin = _mm_loadu_si128((const __m128i*)(input + i));
      uint16_t non_ascii = (uint16_t)_mm_movemask_epi8(latin);
      answer += count_ones(non_ascii);
      latin = _mm_loadu_si128((const __m128i*)(input + i)+1);
      non_ascii = (uint16_t)_mm_movemask_epi8(latin);
      answer += count_ones(non_ascii);
    }
    for(; i + sizeof(__m128i) <= len; i += sizeof(__m128i)) {
      __m128i latin = _mm_loadu_si128((const __m128i*)(input + i));
      uint16_t non_ascii = (uint16_t)_mm_movemask_epi8(latin);
      answer += count_ones(non_ascii);
    }
  }
  return answer + scalar::latin1::utf8_length_from_latin1(reinterpret_cast<const char *>(str + i), len - i);
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
  const __m128i v_00000000 = _mm_setzero_si128();
  const __m128i v_ffffff80 = _mm_set1_epi32((uint32_t)0xffffff80);
  const __m128i v_fffff800 = _mm_set1_epi32((uint32_t)0xfffff800);
  const __m128i v_ffff0000 = _mm_set1_epi32((uint32_t)0xffff0000);
  size_t pos = 0;
  size_t count = 0;
  for(;pos + 4 <= length; pos += 4) {
    __m128i in = _mm_loadu_si128((__m128i*)(input + pos));
    const __m128i ascii_bytes_bytemask = _mm_cmpeq_epi32(_mm_and_si128(in, v_ffffff80), v_00000000);
    const __m128i one_two_bytes_bytemask = _mm_cmpeq_epi32(_mm_and_si128(in, v_fffff800), v_00000000);
    const __m128i two_bytes_bytemask = _mm_xor_si128(one_two_bytes_bytemask, ascii_bytes_bytemask);
    const __m128i one_two_three_bytes_bytemask = _mm_cmpeq_epi32(_mm_and_si128(in, v_ffff0000), v_00000000);
    const __m128i three_bytes_bytemask = _mm_xor_si128(one_two_three_bytes_bytemask, one_two_bytes_bytemask);
    const uint16_t ascii_bytes_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(ascii_bytes_bytemask));
    const uint16_t two_bytes_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(two_bytes_bytemask));
    const uint16_t three_bytes_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(three_bytes_bytemask));

    size_t ascii_count = count_ones(ascii_bytes_bitmask) / 4;
    size_t two_bytes_count = count_ones(two_bytes_bitmask) / 4;
    size_t three_bytes_count = count_ones(three_bytes_bitmask) / 4;
    count += 16 - 3*ascii_count - 2*two_bytes_count - three_bytes_count;
  }
  return count + scalar::utf32::utf8_length_from_utf32(input + pos, length - pos);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  const __m128i v_00000000 = _mm_setzero_si128();
  const __m128i v_ffff0000 = _mm_set1_epi32((uint32_t)0xffff0000);
  size_t pos = 0;
  size_t count = 0;
  for(;pos + 4 <= length; pos += 4) {
    __m128i in = _mm_loadu_si128((__m128i*)(input + pos));
    const __m128i surrogate_bytemask = _mm_cmpeq_epi32(_mm_and_si128(in, v_ffff0000), v_00000000);
    const uint16_t surrogate_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(surrogate_bytemask));
    size_t surrogate_count = (16-count_ones(surrogate_bitmask))/4;
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
  if(options & base64_url) {
    return encode_base64<true>(output, input, length, options);
  } else {
    return encode_base64<false>(output, input, length, options);
  }
}
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/westmere/end.h"
