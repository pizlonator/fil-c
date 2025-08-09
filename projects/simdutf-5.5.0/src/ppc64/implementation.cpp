#include "scalar/utf8.h"
#include "scalar/utf16.h"
#include "scalar/latin1.h"
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

#include "simdutf/ppc64/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_PPC64_H
#error "ppc64.h must be included"
#endif
using namespace simd;


simdutf_really_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  // careful: 0x80 is not ascii.
  return input.reduce_or().saturating_sub(0b01111111u).bits_not_set_anywhere();
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
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<bool>(is_third_byte | is_fourth_byte);
}

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
    if(validate_utf16(reinterpret_cast<const char16_t*>(input), length/2)) { out |= encoding_type::UTF16_LE; }
  }
  if((length % 4) == 0) {
    if(validate_utf32(reinterpret_cast<const char32_t*>(input), length/4)) { out |= encoding_type::UTF32_LE; }
  }

  return out;
}

simdutf_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return ppc64::utf8_validation::generic_validate_utf8(buf,len);
}

simdutf_warn_unused result implementation::validate_utf8_with_errors(const char *buf, size_t len) const noexcept {
  return ppc64::utf8_validation::generic_validate_utf8_with_errors(buf,len);
}

simdutf_warn_unused bool implementation::validate_ascii(const char *buf, size_t len) const noexcept {
  return ppc64::utf8_validation::generic_validate_ascii(buf,len);
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(const char *buf, size_t len) const noexcept {
  return ppc64::utf8_validation::generic_validate_ascii_with_errors(buf,len);
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

simdutf_warn_unused result implementation::validate_utf32_with_errors(const char32_t *buf, size_t len) const noexcept {
  return scalar::utf32::validate_with_errors(buf, len);
}

simdutf_warn_unused bool implementation::validate_utf32(const char16_t *buf, size_t len) const noexcept {
  return scalar::utf32::validate(buf, len);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16le(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return 0; // stub
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16be(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return 0; // stub
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16le_with_errors(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return result(error_code::OTHER, 0); // stub
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16be_with_errors(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return result(error_code::OTHER, 0); // stub
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16le(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return 0; // stub
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16be(const char* /*buf*/, size_t /*len*/, char16_t* /*utf16_output*/) const noexcept {
  return 0; // stub
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf32(const char* /*buf*/, size_t /*len*/, char32_t* /*utf16_output*/) const noexcept {
  return 0; // stub
}

simdutf_warn_unused result implementation::convert_utf8_to_utf32_with_errors(const char* /*buf*/, size_t /*len*/, char32_t* /*utf16_output*/) const noexcept {
  return result(error_code::OTHER, 0); // stub
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf32(const char* /*buf*/, size_t /*len*/, char32_t* /*utf16_output*/) const noexcept {
  return 0; // stub
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
  return utf8::count_code_points(input, length);
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

simdutf_warn_unused size_t implementation::utf16_length_from_utf8(const char * input, size_t length) const noexcept {
  return scalar::utf8::utf16_length_from_utf8(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  return scalar::utf32::utf8_length_from_utf32(input, length);
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf32(const char32_t * input, size_t length) const noexcept {
  return scalar::utf32::utf16_length_from_utf32(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf8(const char * input, size_t length) const noexcept {
  return scalar::utf8::count_code_points(input, length);
}


simdutf_warn_unused size_t implementation::maximal_binary_length_from_base64(const char * input, size_t length) const noexcept {
  return scalar::base64::maximal_binary_length_from_base64(input, length);
}

simdutf_warn_unused result implementation::base64_to_binary(const char * input, size_t length, char* output, base64_options options) const noexcept {
  // skip trailing spaces
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
      return {INVALID_BASE64_CHARACTER, equallocation};;
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
  // skip trailing spaces
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
  return scalar::base64::binary_to_base64(input, length, output, options);
}
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/ppc64/end.h"
