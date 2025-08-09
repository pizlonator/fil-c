#ifndef SIMDUTF_BASE64_H
#define SIMDUTF_BASE64_H

#include <cstddef>
#include <cstdint>
#include <cstring>
namespace simdutf {
namespace scalar {
namespace {
namespace base64 {

// This function is not expected to be fast. Do not use in long loops.
template <class char_type>
bool is_ascii_white_space(char_type c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

template <class char_type>
bool is_eight_byte(char_type c) {
  if(sizeof(char_type) == 1) {
    return true;
  }
  return uint8_t(c) == c;
}

// Returns true upon success. The destination buffer must be large enough.
// This functions assumes that the padding (=) has been removed.
template <class char_type>
result base64_tail_decode(char *dst, const char_type *src, size_t length, base64_options options) {
  // This looks like 5 branches, but we expect the compiler to resolve this to a single branch:
  const uint8_t *to_base64 = (options & base64_url) ? tables::base64::to_base64_url_value : tables::base64::to_base64_value;
  const uint32_t *d0 = (options & base64_url) ? tables::base64::base64_url::d0 : tables::base64::base64_default::d0;
  const uint32_t *d1 = (options & base64_url) ? tables::base64::base64_url::d1 : tables::base64::base64_default::d1;
  const uint32_t *d2 = (options & base64_url) ? tables::base64::base64_url::d2 : tables::base64::base64_default::d2;
  const uint32_t *d3 = (options & base64_url) ? tables::base64::base64_url::d3 : tables::base64::base64_default::d3;

  const char_type *srcend = src + length;
  const char_type *srcinit = src;
  const char *dstinit = dst;

  uint32_t x;
  size_t idx;
  uint8_t buffer[4];
  while (true) {
    while (src + 4 <= srcend && is_eight_byte(src[0]) && is_eight_byte(src[1]) && is_eight_byte(src[2]) && is_eight_byte(src[3]) &&
           (x = d0[uint8_t(src[0])] | d1[uint8_t(src[1])] |
                d2[uint8_t(src[2])] | d3[uint8_t(src[3])]) < 0x01FFFFFF) {
      if(match_system(endianness::BIG)) {
        x = scalar::utf32::swap_bytes(x);
      }
      std::memcpy(dst, &x, 3); // optimization opportunity: copy 4 bytes
      dst += 3;
      src += 4;
    }
    idx = 0;
    // we need at least four characters.
    while (idx < 4 && src < srcend) {
      char_type c = *src;
      uint8_t code = to_base64[uint8_t(c)];
      buffer[idx] = uint8_t(code);
      if (is_eight_byte(c) && code <= 63) {
        idx++;
      } else if (code > 64 || !scalar::base64::is_eight_byte(c)) {
        return {INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
      } else {
        // We have a space or a newline. We ignore it.
      }
      src++;
    }
    if (idx != 4) {
      if (idx == 2) {
        uint32_t triple =
            (uint32_t(buffer[0]) << 3 * 6) + (uint32_t(buffer[1]) << 2 * 6);
        if(match_system(endianness::BIG)) {
          triple <<= 8;
          std::memcpy(dst, &triple, 1);
        } else {
          triple = scalar::utf32::swap_bytes(triple);
          triple >>= 8;
          std::memcpy(dst, &triple, 1);
        }
        dst += 1;

      } else if (idx == 3) {
        uint32_t triple = (uint32_t(buffer[0]) << 3 * 6) +
                          (uint32_t(buffer[1]) << 2 * 6) +
                          (uint32_t(buffer[2]) << 1 * 6);
        if(match_system(endianness::BIG)) {
          triple <<= 8;
          std::memcpy(dst, &triple, 2);
        } else {
          triple = scalar::utf32::swap_bytes(triple);
          triple >>= 8;
          std::memcpy(dst, &triple, 2);
        }
        dst += 2;
      } else if (idx == 1) {
        return {BASE64_INPUT_REMAINDER, size_t(dst - dstinit)};
      }
      return {SUCCESS, size_t(dst - dstinit)};
    }

    uint32_t triple =
        (uint32_t(buffer[0]) << 3 * 6) + (uint32_t(buffer[1]) << 2 * 6) +
        (uint32_t(buffer[2]) << 1 * 6) + (uint32_t(buffer[3]) << 0 * 6);
    if(match_system(endianness::BIG)) {
      triple <<= 8;
      std::memcpy(dst, &triple, 3);
    } else {
      triple = scalar::utf32::swap_bytes(triple);
      triple >>= 8;
      std::memcpy(dst, &triple, 3);
    }
    dst += 3;
  }
}

// like base64_tail_decode, but it will not write past the end of the output buffer.
// outlen is modified to reflect the number of bytes written.
// This functions assumes that the padding (=) has been removed.
template <class char_type>
result base64_tail_decode_safe(char *dst, size_t& outlen, const char_type *src, size_t length, base64_options options) {
  if(length == 0) {
    outlen = 0;
    return {SUCCESS, 0};
  }
  // This looks like 5 branches, but we expect the compiler to resolve this to a single branch:
  const uint8_t *to_base64 = (options & base64_url) ? tables::base64::to_base64_url_value : tables::base64::to_base64_value;
  const uint32_t *d0 = (options & base64_url) ? tables::base64::base64_url::d0 : tables::base64::base64_default::d0;
  const uint32_t *d1 = (options & base64_url) ? tables::base64::base64_url::d1 : tables::base64::base64_default::d1;
  const uint32_t *d2 = (options & base64_url) ? tables::base64::base64_url::d2 : tables::base64::base64_default::d2;
  const uint32_t *d3 = (options & base64_url) ? tables::base64::base64_url::d3 : tables::base64::base64_default::d3;

  const char_type *srcend = src + length;
  const char_type *srcinit = src;
  const char *dstinit = dst;
  const char *dstend = dst + outlen;

  uint32_t x;
  size_t idx;
  uint8_t buffer[4];
  while (true) {
    while (src + 4 <= srcend && is_eight_byte(src[0]) && is_eight_byte(src[1]) && is_eight_byte(src[2]) && is_eight_byte(src[3]) &&
           (x = d0[uint8_t(src[0])] | d1[uint8_t(src[1])] |
                d2[uint8_t(src[2])] | d3[uint8_t(src[3])]) < 0x01FFFFFF) {
      if(match_system(endianness::BIG)) {
        x = scalar::utf32::swap_bytes(x);
      }
      if(dstend - dst < 3) {
        outlen = size_t(dst - dstinit);
        return {OUTPUT_BUFFER_TOO_SMALL, size_t(src - srcinit)};
      }
      std::memcpy(dst, &x, 3); // optimization opportunity: copy 4 bytes
      dst += 3;
      src += 4;
    }
    idx = 0;
    const char_type *srccur = src;

    // we need at least four characters.
    while (idx < 4 && src < srcend) {
      char_type c = *src;
      uint8_t code = to_base64[uint8_t(c)];
      buffer[idx] = uint8_t(code);
      if (is_eight_byte(c) && code <= 63) {
        idx++;
      } else if (code > 64 || !scalar::base64::is_eight_byte(c)) {
        outlen = size_t(dst - dstinit);
        return {INVALID_BASE64_CHARACTER, size_t(src - srcinit)};
      } else {
        // We have a space or a newline. We ignore it.
      }
      src++;
    }
    if (idx != 4) {
      if (idx == 2) {
        if(dst == dstend) {
          outlen = size_t(dst - dstinit);
          return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit)};
        }
        uint32_t triple =
            (uint32_t(buffer[0]) << 3 * 6) + (uint32_t(buffer[1]) << 2 * 6);
        if(match_system(endianness::BIG)) {
          triple <<= 8;
          std::memcpy(dst, &triple, 1);
        } else {
          triple = scalar::utf32::swap_bytes(triple);
          triple >>= 8;
          std::memcpy(dst, &triple, 1);
        }
        dst += 1;

      } else if (idx == 3) {
        if(dstend - dst < 2) {
          outlen = size_t(dst - dstinit);
          return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit)};
        }
        uint32_t triple = (uint32_t(buffer[0]) << 3 * 6) +
                          (uint32_t(buffer[1]) << 2 * 6) +
                          (uint32_t(buffer[2]) << 1 * 6);
        if(match_system(endianness::BIG)) {
          triple <<= 8;
          std::memcpy(dst, &triple, 2);
        } else {
          triple = scalar::utf32::swap_bytes(triple);
          triple >>= 8;
          std::memcpy(dst, &triple, 2);
        }
        dst += 2;
      } else if (idx == 1) {
        outlen = size_t(dst - dstinit);
        return {BASE64_INPUT_REMAINDER, size_t(dst - dstinit)};
      }
      outlen = size_t(dst - dstinit);
      return {SUCCESS, size_t(dst - dstinit)};
    }
    if(dstend - dst < 3) {
      outlen = size_t(dst - dstinit);
      return {OUTPUT_BUFFER_TOO_SMALL, size_t(srccur - srcinit)};
    }
    uint32_t triple =
        (uint32_t(buffer[0]) << 3 * 6) + (uint32_t(buffer[1]) << 2 * 6) +
        (uint32_t(buffer[2]) << 1 * 6) + (uint32_t(buffer[3]) << 0 * 6);
    if(match_system(endianness::BIG)) {
      triple <<= 8;
      std::memcpy(dst, &triple, 3);
    } else {
      triple = scalar::utf32::swap_bytes(triple);
      triple >>= 8;
      std::memcpy(dst, &triple, 3);
    }
    dst += 3;
  }
}

// Returns the number of bytes written. The destination buffer must be large
// enough. It will add padding (=) if needed.
size_t tail_encode_base64(char *dst, const char *src, size_t srclen, base64_options options) {
  // By default, we use padding if we are not using the URL variant.
  // This is check with ((options & base64_url) == 0) which returns true if we are not using the URL variant.
  // However, we also allow 'inversion' of the convention with the base64_reverse_padding option.
  // If the base64_reverse_padding option is set, we use padding if we are using the URL variant,
  // and we omit it if we are not using the URL variant. This is checked with
  // ((options & base64_reverse_padding) == base64_reverse_padding).
  bool use_padding = ((options & base64_url) == 0) ^ ((options & base64_reverse_padding) == base64_reverse_padding);
  // This looks like 3 branches, but we expect the compiler to resolve this to a single branch:
  const char *e0 = (options & base64_url) ? tables::base64::base64_url::e0 : tables::base64::base64_default::e0;
  const char *e1 = (options & base64_url) ? tables::base64::base64_url::e1 : tables::base64::base64_default::e1;
  const char *e2 = (options & base64_url) ? tables::base64::base64_url::e2 : tables::base64::base64_default::e2;
  char *out = dst;
  size_t i = 0;
  uint8_t t1, t2, t3;
  for (; i + 2 < srclen; i += 3) {
    t1 = uint8_t(src[i]);
    t2 = uint8_t(src[i + 1]);
    t3 = uint8_t(src[i + 2]);
    *out++ = e0[t1];
    *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
    *out++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
    *out++ = e2[t3];
  }
  switch (srclen - i) {
  case 0:
    break;
  case 1:
    t1 = uint8_t(src[i]);
    *out++ = e0[t1];
    *out++ = e1[(t1 & 0x03) << 4];
    if(use_padding) {
      *out++ = '=';
      *out++ = '=';
    }
    break;
  default: /* case 2 */
    t1 = uint8_t(src[i]);
    t2 = uint8_t(src[i + 1]);
    *out++ = e0[t1];
    *out++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
    *out++ = e2[(t2 & 0x0F) << 2];
    if(use_padding) {
      *out++ = '=';
    }
  }
  return (size_t)(out - dst);
}

template <class char_type>
simdutf_warn_unused size_t maximal_binary_length_from_base64(const char_type * input, size_t length) noexcept {
  // We follow https://infra.spec.whatwg.org/#forgiving-base64-decode
  size_t padding = 0;
  if(length > 0) {
    if(input[length - 1] == '=') {
      padding++;
      if(length > 1 && input[length - 2] == '=') {
        padding++;
      }
    }
  }
  size_t actual_length = length - padding;
  if(actual_length % 4 <= 1) {
    return actual_length / 4 * 3;
  }
  // if we have a valid input, then the remainder must be 2 or 3 adding one or two extra bytes.
  return  actual_length / 4 * 3 + (actual_length %4)  - 1;
}

simdutf_warn_unused size_t base64_length_from_binary(size_t length, base64_options options) noexcept {
  // By default, we use padding if we are not using the URL variant.
  // This is check with ((options & base64_url) == 0) which returns true if we are not using the URL variant.
  // However, we also allow 'inversion' of the convention with the base64_reverse_padding option.
  // If the base64_reverse_padding option is set, we use padding if we are using the URL variant,
  // and we omit it if we are not using the URL variant. This is checked with
  // ((options & base64_reverse_padding) == base64_reverse_padding).
  bool use_padding = ((options & base64_url) == 0) ^ ((options & base64_reverse_padding) == base64_reverse_padding);
  if(!use_padding) {
    return length/3 * 4 + ((length % 3) ? (length % 3) + 1 : 0);
  }
  return (length + 2)/3 * 4; // We use padding to make the length a multiple of 4.
}

} // namespace base64
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
