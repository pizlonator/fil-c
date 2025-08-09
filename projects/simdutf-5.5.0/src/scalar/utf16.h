#ifndef SIMDUTF_UTF16_H
#define SIMDUTF_UTF16_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16 {

inline simdutf_warn_unused uint16_t swap_bytes(const uint16_t word) {
  return uint16_t((word >> 8) | (word << 8));
}

template <endianness big_endian>
inline simdutf_warn_unused bool validate(const char16_t *buf, size_t len) noexcept {
  const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  uint64_t pos = 0;
  while (pos < len) {
    uint16_t word = !match_system(big_endian) ? swap_bytes(data[pos]) : data[pos];
    if((word &0xF800) == 0xD800) {
        if(pos + 1 >= len) { return false; }
        uint16_t diff = uint16_t(word - 0xD800);
        if(diff > 0x3FF) { return false; }
        uint16_t next_word = !match_system(big_endian) ? swap_bytes(data[pos + 1]) : data[pos + 1];
        uint16_t diff2 = uint16_t(next_word - 0xDC00);
        if(diff2 > 0x3FF) { return false; }
        pos += 2;
    } else {
        pos++;
    }
  }
  return true;
}

template <endianness big_endian>
inline simdutf_warn_unused result validate_with_errors(const char16_t *buf, size_t len) noexcept {
  const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  size_t pos = 0;
  while (pos < len) {
    uint16_t word = !match_system(big_endian) ? swap_bytes(data[pos]) : data[pos];
    if((word & 0xF800) == 0xD800) {
        if(pos + 1 >= len) { return result(error_code::SURROGATE, pos); }
        uint16_t diff = uint16_t(word - 0xD800);
        if(diff > 0x3FF) { return result(error_code::SURROGATE, pos); }
        uint16_t next_word = !match_system(big_endian) ? swap_bytes(data[pos + 1]) : data[pos + 1];
        uint16_t diff2 = uint16_t(next_word - 0xDC00);
        if(diff2 > 0x3FF) { return result(error_code::SURROGATE, pos); }
        pos += 2;
    } else {
        pos++;
    }
  }
  return result(error_code::SUCCESS, pos);
}

template <endianness big_endian>
inline size_t count_code_points(const char16_t* buf, size_t len) {
  // We are not BOM aware.
  const uint16_t * p = reinterpret_cast<const uint16_t *>(buf);
  size_t counter{0};
  for(size_t i = 0; i < len; i++) {
    uint16_t word = !match_system(big_endian) ? swap_bytes(p[i]) : p[i];
    counter += ((word & 0xFC00) != 0xDC00);
  }
  return counter;
}

template <endianness big_endian>
inline size_t utf8_length_from_utf16(const char16_t* buf, size_t len) {
  // We are not BOM aware.
  const uint16_t * p = reinterpret_cast<const uint16_t *>(buf);
  size_t counter{0};
  for(size_t i = 0; i < len; i++) {
    uint16_t word = !match_system(big_endian) ? swap_bytes(p[i]) : p[i];
    counter++;                                      // ASCII
    counter += static_cast<size_t>(word > 0x7F);    // non-ASCII is at least 2 bytes, surrogates are 2*2 == 4 bytes
    counter += static_cast<size_t>((word > 0x7FF && word <= 0xD7FF) || (word >= 0xE000));   // three-byte
  }
  return counter;
}

template <endianness big_endian>
inline size_t utf32_length_from_utf16(const char16_t* buf, size_t len) {
  // We are not BOM aware.
  const uint16_t * p = reinterpret_cast<const uint16_t *>(buf);
  size_t counter{0};
  for(size_t i = 0; i < len; i++) {
    uint16_t word = !match_system(big_endian) ? swap_bytes(p[i]) : p[i];
    counter += ((word & 0xFC00) != 0xDC00);
  }
  return counter;
}


inline size_t latin1_length_from_utf16(size_t len) {
  return len;
}

simdutf_really_inline void change_endianness_utf16(const char16_t* in, size_t size, char16_t* out) {
  const uint16_t * input = reinterpret_cast<const uint16_t *>(in);
  uint16_t * output = reinterpret_cast<uint16_t *>(out);
  for (size_t i = 0; i < size; i++) {
    *output++ = uint16_t(input[i] >> 8 | input[i] << 8);
  }
}


template <endianness big_endian>
simdutf_warn_unused inline size_t trim_partial_utf16(const char16_t* input, size_t length) {
  if (length <= 1) {
    return length;
  }
  uint16_t last_word = uint16_t(input[length-1]);
  last_word = !match_system(big_endian) ? swap_bytes(last_word) : last_word;
  length -= ((last_word & 0xFC00) == 0xD800);
  return length;
}

} // utf16 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif