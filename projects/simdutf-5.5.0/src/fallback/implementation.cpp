#include "simdutf/fallback/begin.h"

#include "scalar/utf8_to_utf16/valid_utf8_to_utf16.h"
#include "scalar/utf8_to_utf16/utf8_to_utf16.h"

#include "scalar/utf8_to_utf32/valid_utf8_to_utf32.h"
#include "scalar/utf8_to_utf32/utf8_to_utf32.h"

#include "scalar/utf8_to_latin1/valid_utf8_to_latin1.h"
#include "scalar/utf8_to_latin1/utf8_to_latin1.h"

#include "scalar/utf16_to_utf8/valid_utf16_to_utf8.h"
#include "scalar/utf16_to_utf8/utf16_to_utf8.h"

#include "scalar/utf16_to_utf32/valid_utf16_to_utf32.h"
#include "scalar/utf16_to_utf32/utf16_to_utf32.h"

#include "scalar/utf32_to_utf8/valid_utf32_to_utf8.h"
#include "scalar/utf32_to_utf8/utf32_to_utf8.h"

#include "scalar/utf32_to_utf16/valid_utf32_to_utf16.h"
#include "scalar/utf32_to_utf16/utf32_to_utf16.h"

#include "scalar/ascii.h"
#include "scalar/base64.h"
#include "scalar/utf8.h"
#include "scalar/utf16.h"
#include "scalar/latin1.h"
#include "scalar/utf8_to_latin1/valid_utf8_to_latin1.h"
#include "scalar/utf8_to_latin1/utf8_to_latin1.h"
#include <cstdint>
#include <cstring>

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
  return scalar::utf8::validate(buf, len);
}

simdutf_warn_unused result implementation::validate_utf8_with_errors(const char *buf, size_t len) const noexcept {
  return scalar::utf8::validate_with_errors(buf, len);
}

simdutf_warn_unused bool implementation::validate_ascii(const char *buf, size_t len) const noexcept {
  return scalar::ascii::validate(buf, len);
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(const char *buf, size_t len) const noexcept {
  return scalar::ascii::validate_with_errors(buf, len);
}

simdutf_warn_unused bool implementation::validate_utf16le(const char16_t *buf, size_t len) const noexcept {
  return scalar::utf16::validate<endianness::LITTLE>(buf, len);
}

simdutf_warn_unused bool implementation::validate_utf16be(const char16_t *buf, size_t len) const noexcept {
  return scalar::utf16::validate<endianness::BIG>(buf, len);
}

simdutf_warn_unused result implementation::validate_utf16le_with_errors(const char16_t *buf, size_t len) const noexcept {
  return scalar::utf16::validate_with_errors<endianness::LITTLE>(buf, len);
}

simdutf_warn_unused result implementation::validate_utf16be_with_errors(const char16_t *buf, size_t len) const noexcept {
  return scalar::utf16::validate_with_errors<endianness::BIG>(buf, len);
}

simdutf_warn_unused bool implementation::validate_utf32(const char32_t *buf, size_t len) const noexcept {
  return scalar::utf32::validate(buf, len);
}

simdutf_warn_unused result implementation::validate_utf32_with_errors(const char32_t *buf, size_t len) const noexcept {
  return scalar::utf32::validate_with_errors(buf, len);
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf8(const char * buf, size_t len, char* utf8_output) const noexcept {
  return scalar::latin1_to_utf8::convert(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::latin1_to_utf16::convert<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::latin1_to_utf16::convert<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf32(const char * buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::latin1_to_utf32::convert(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_latin1(const char* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf8_to_latin1::convert(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_latin1_with_errors(const char* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf8_to_latin1::convert_with_errors(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_latin1(const char* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf8_to_latin1::convert_valid(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16le_with_errors(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert_with_errors<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16be_with_errors(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert_with_errors<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16le(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert_valid<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16be(const char* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf8_to_utf16::convert_valid<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf32(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf8_to_utf32::convert(buf, len, utf32_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf32_with_errors(const char* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf8_to_utf32::convert_with_errors(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf32(const char* input, size_t size,
    char32_t* utf32_output) const noexcept {
  return scalar::utf8_to_utf32::convert_valid(input, size,  utf32_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert<endianness::LITTLE>(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert<endianness::BIG>(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf16le_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert_with_errors<endianness::LITTLE>(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf16be_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert_with_errors<endianness::BIG>(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert_valid<endianness::LITTLE>(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_latin1(const char16_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf16_to_latin1::convert_valid<endianness::BIG>(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert<endianness::LITTLE>(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert<endianness::BIG>(buf, len, utf8_output);
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert_with_errors<endianness::LITTLE>(buf, len, utf8_output);
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert_with_errors<endianness::BIG>(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert_valid<endianness::LITTLE>(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf8(const char16_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf16_to_utf8::convert_valid<endianness::BIG>(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_latin1(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf32_to_latin1::convert(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf32_to_latin1_with_errors(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf32_to_latin1::convert_with_errors(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_latin1(const char32_t* buf, size_t len, char* latin1_output) const noexcept {
  return scalar::utf32_to_latin1::convert_valid(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf32_to_utf8::convert(buf, len, utf8_output);
}

simdutf_warn_unused result implementation::convert_utf32_to_utf8_with_errors(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf32_to_utf8::convert_with_errors(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output) const noexcept {
  return scalar::utf32_to_utf8::convert_valid(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16le(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16be(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16le_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert_with_errors<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16be_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert_with_errors<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16le(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert_valid<endianness::LITTLE>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16be(const char32_t* buf, size_t len, char16_t* utf16_output) const noexcept {
  return scalar::utf32_to_utf16::convert_valid<endianness::BIG>(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert<endianness::LITTLE>(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert<endianness::BIG>(buf, len, utf32_output);
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf32_with_errors(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert_with_errors<endianness::LITTLE>(buf, len, utf32_output);
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf32_with_errors(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert_with_errors<endianness::BIG>(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert_valid<endianness::LITTLE>(buf, len, utf32_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) const noexcept {
  return scalar::utf16_to_utf32::convert_valid<endianness::BIG>(buf, len, utf32_output);
}

void implementation::change_endianness_utf16(const char16_t * input, size_t length, char16_t * output) const noexcept {
  scalar::utf16::change_endianness_utf16(input, length, output);
}

simdutf_warn_unused size_t implementation::count_utf16le(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::count_code_points<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::count_utf16be(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::count_code_points<endianness::BIG>(input, length);
}

simdutf_warn_unused size_t implementation::count_utf8(const char * input, size_t length) const noexcept {
  return scalar::utf8::count_code_points(input, length);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf8(const char* buf, size_t len) const noexcept {
  return scalar::utf8::count_code_points(buf,len);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf16(size_t length) const noexcept {
  return scalar::utf16::latin1_length_from_utf16(length);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf32(size_t length) const noexcept {
  return length;
}

simdutf_warn_unused size_t implementation::utf8_length_from_latin1(const char * input, size_t length) const noexcept {
  size_t answer = length;
  size_t i = 0;
  auto pop = [](uint64_t v) {
    return (size_t)(((v>>7) & UINT64_C(0x0101010101010101)) * UINT64_C(0x0101010101010101) >> 56);
  };
  for(; i + 32 <= length; i += 32) {
    uint64_t v;
    memcpy(&v, input + i, 8);
    answer += pop(v);
    memcpy(&v, input + i + 8, sizeof(v));
    answer += pop(v);
    memcpy(&v, input + i + 16, sizeof(v));
    answer += pop(v);
    memcpy(&v, input + i + 24, sizeof(v));
    answer += pop(v);
  }
  for(; i + 8 <= length; i += 8) {
    uint64_t v;
    memcpy(&v, input + i, sizeof(v));
    answer += pop(v);
  }
  for(; i + 1 <= length; i += 1) {
      answer += static_cast<uint8_t>(input[i]) >> 7;
  }
  return answer;
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16le(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::utf8_length_from_utf16<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16be(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::utf8_length_from_utf16<endianness::BIG>(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16le(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::utf32_length_from_utf16<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16be(const char16_t * input, size_t length) const noexcept {
  return scalar::utf16::utf32_length_from_utf16<endianness::BIG>(input, length);
}

simdutf_warn_unused size_t implementation::utf16_length_from_latin1(size_t length) const noexcept {
  return scalar::latin1::utf16_length_from_latin1(length);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf8(const char * input, size_t length) const noexcept {
  return scalar::utf8::utf16_length_from_utf8(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  return scalar::utf32::utf8_length_from_utf32(input, length);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  return scalar::utf32::utf16_length_from_utf32(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_latin1(size_t length) const noexcept {
  return scalar::latin1::utf32_length_from_latin1(length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf8(const char * input, size_t length) const noexcept {
  return scalar::utf8::count_code_points(input, length);
}

simdutf_warn_unused size_t implementation::maximal_binary_length_from_base64(const char * input, size_t length) const noexcept {
  return scalar::base64::maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result implementation::base64_to_binary(const char * input, size_t length, char* output, base64_options options) const noexcept {
  while(length > 0 && scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation = length; // location of the first padding character if any
  size_t equalsigns = 0;
  if(length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while(length > 0 && scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if(length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if(length == 0) {
    if(equalsigns > 0) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
    return {SUCCESS, 0};
  }
  result r = scalar::base64::base64_tail_decode(output, input, length, options);
  if(r.error == error_code::SUCCESS && equalsigns > 0) {
    // additional checks
    if((r.count % 3 == 0) || ((r.count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
  }
  return r;
}

simdutf_warn_unused size_t implementation::maximal_binary_length_from_base64(const char16_t * input, size_t length) const noexcept {
  return scalar::base64::maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result implementation::base64_to_binary(const char16_t * input, size_t length, char* output, base64_options options) const noexcept {
  while(length > 0 && scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation = length; // location of the first padding character if any
  size_t equalsigns = 0;
  if(length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while(length > 0 && scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if(length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if(length == 0) {
    if(equalsigns > 0) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
    return {SUCCESS, 0};
  }
  result r = scalar::base64::base64_tail_decode(output, input, length, options);
  if(r.error == error_code::SUCCESS && equalsigns > 0) {
    // additional checks
    if((r.count % 3 == 0) || ((r.count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
  }
  return r;
}

simdutf_warn_unused size_t implementation::base64_length_from_binary(size_t length, base64_options options) const noexcept {
  return scalar::base64::base64_length_from_binary(length, options);
}

size_t implementation::binary_to_base64(const char * input, size_t length, char* output, base64_options options) const noexcept {
  return scalar::base64::tail_encode_base64(output, input, length, options);
}
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/fallback/end.h"
