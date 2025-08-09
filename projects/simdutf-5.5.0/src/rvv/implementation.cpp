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

#include "simdutf/rvv/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_RVV_H
#error "rvv.h must be included"
#endif

} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

//
// Implementation-specific overrides
//
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {

#include "rvv/rvv_length_from.inl.cpp"
#include "rvv/rvv_validate.inl.cpp"

#include "rvv/rvv_latin1_to.inl.cpp"
#include "rvv/rvv_utf8_to.inl.cpp"
#include "rvv/rvv_utf16_to.inl.cpp"
#include "rvv/rvv_utf32_to.inl.cpp"

simdutf_warn_unused int implementation::detect_encodings(const char *input, size_t length) const noexcept {
  // If there is a BOM, then we trust it.
  auto bom_encoding = simdutf::BOM::check_bom(input, length);
  if (bom_encoding != encoding_type::unspecified)
    return bom_encoding;
  // todo: reimplement as a one-pass algorithm.
  int out = 0;
  if (validate_utf8(input, length))
    out |= encoding_type::UTF8;
  if (length % 2 == 0) {
    if (validate_utf16(reinterpret_cast<const char16_t*>(input), length/2))
      out |= encoding_type::UTF16_LE;
  }
  if (length % 4 == 0) {
    if (validate_utf32(reinterpret_cast<const char32_t*>(input), length/4))
      out |= encoding_type::UTF32_LE;
  }

  return out;
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static void rvv_change_endianness_utf16(const char16_t *src, size_t len, char16_t *dst) {
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e16m8(len);
    vuint16m8_t v = __riscv_vle16_v_u16m8((uint16_t*)src, vl);
    __riscv_vse16_v_u16m8((uint16_t *)dst, simdutf_byteflip<bflip>(v, vl), vl);
  }
}

void implementation::change_endianness_utf16(const char16_t *src, size_t len, char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_change_endianness_utf16<simdutf_ByteFlip::ZVBB>(src, len, dst);
  else
    return rvv_change_endianness_utf16<simdutf_ByteFlip::V>(src, len, dst);
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
  auto equalsigns = 0;
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

#include "simdutf/rvv/end.h"
