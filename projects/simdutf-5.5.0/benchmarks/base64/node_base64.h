// Taken from Node.js, for benchmarking purposes.
// See https://github.com/nodejs/node/blob/dafe004703f4f0a878558090ab3dd8ef0624037d/src/base64.h#L15
// and https://github.com/nodejs/node/blob/dafe004703f4f0a878558090ab3dd8ef0624037d/src/base64-inl.h
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace node {
//// Base 64 ////

enum class Base64Mode { NORMAL, URL };

// Doesn't check for padding at the end.  Can be 1-2 bytes over.
inline constexpr size_t base64_decoded_size_fast(size_t size) {
  // 1-byte input cannot be decoded
  return size > 1 ? (size / 4) * 3 + (size % 4 + 1) / 2 : 0;
}

inline uint32_t ReadUint32BE(const unsigned char *p);

template <typename TypeName>
size_t base64_decoded_size(const TypeName *src, size_t size);

template <typename TypeName>
size_t base64_decode(char *const dst, const size_t dstlen,
                     const TypeName *const src, const size_t srclen);

inline size_t base64_encode(const char *src, size_t slen, char *dst,
                            size_t dlen, Base64Mode mode = Base64Mode::NORMAL);
} // namespace node

namespace node {
/*
static constexpr char base64_table_url[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                           "abcdefghijklmnopqrstuvwxyz"
                                           "0123456789-_";*/

/*extern const int8_t unbase64_table[256];*/

// supports regular and URL-safe base64
const int8_t unbase64_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 62, -1, 62, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1,
    63, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
    43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

inline int8_t unbase64(uint8_t x) { return unbase64_table[x]; }

inline uint32_t ReadUint32BE(const unsigned char *p) {
  return static_cast<uint32_t>(p[0] << 24U) |
         static_cast<uint32_t>(p[1] << 16U) |
         static_cast<uint32_t>(p[2] << 8U) | static_cast<uint32_t>(p[3]);
}

#ifdef _MSC_VER
#pragma warning(push)
// MSVC C4003: not enough actual parameters for macro 'identifier'
#pragma warning(disable : 4003)
#endif
template <typename TypeName>
bool base64_decode_group_slow(char *const dst, const size_t dstlen,
                              const TypeName *const src, const size_t srclen,
                              size_t *const i, size_t *const k) {
  uint8_t hi;
  uint8_t lo;
#define V(expr)                                                                \
  for (;;) {                                                                   \
    const uint8_t c = static_cast<uint8_t>(src[*i]);                           \
    lo = uint8_t(unbase64(c));                                                 \
    *i += 1;                                                                   \
    if (lo < 64)                                                               \
      break; /* Legal character. */                                            \
    if (c == '=' || *i >= srclen)                                              \
      return false; /* Stop decoding. */                                       \
  }                                                                            \
  expr;                                                                        \
  if (*i >= srclen)                                                            \
    return false;                                                              \
  if (*k >= dstlen)                                                            \
    return false;                                                              \
  hi = lo;
  V(/* Nothing. */);
  V(dst[(*k)++] = char((hi & 0x3F) << 2) | ((lo & 0x30) >> 4));
  V(dst[(*k)++] = char((hi & 0x0F) << 4) | ((lo & 0x3C) >> 2));
  V(dst[(*k)++] = char((hi & 0x03) << 6) | ((lo & 0x3F) >> 0));
#undef V
  return true; // Continue decoding.
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename TypeName>
size_t base64_decode_fast(char *const dst, const size_t dstlen,
                          const TypeName *const src, const size_t srclen,
                          const size_t decoded_size) {
  const size_t available = dstlen < decoded_size ? dstlen : decoded_size;
  const size_t max_k = available / 3 * 3;
  size_t max_i = srclen / 4 * 4;
  size_t i = 0;
  size_t k = 0;
  while (i < max_i && k < max_k) {
    const unsigned char txt[] = {
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 0]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 1]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 2]))),
        static_cast<unsigned char>(unbase64(static_cast<uint8_t>(src[i + 3]))),
    };

    const uint32_t v = ReadUint32BE(txt);
    // If MSB is set, input contains whitespace or is not valid base64.
    if (v & 0x80808080) {
      if (!base64_decode_group_slow(dst, dstlen, src, srclen, &i, &k))
        return k;
      max_i = i + (srclen - i) / 4 * 4; // Align max_i again.
    } else {
      dst[k + 0] = char(((v >> 22) & 0xFC) | ((v >> 20) & 0x03));
      dst[k + 1] = char(((v >> 12) & 0xF0) | ((v >> 10) & 0x0F));
      dst[k + 2] = char(((v >> 2) & 0xC0) | ((v >> 0) & 0x3F));
      i += 4;
      k += 3;
    }
  }
  if (i < srclen && k < dstlen) {
    base64_decode_group_slow(dst, dstlen, src, srclen, &i, &k);
  }
  return k;
}

template <typename TypeName>
size_t base64_decoded_size(const TypeName *src, size_t size) {
  // 1-byte input cannot be decoded
  if (size < 2)
    return 0;

  if (src[size - 1] == '=') {
    size--;
    if (src[size - 1] == '=')
      size--;
  }
  return base64_decoded_size_fast(size);
}

template <typename TypeName>
size_t base64_decode(char *const dst, const size_t dstlen,
                     const TypeName *const src, const size_t srclen) {
  const size_t decoded_size = base64_decoded_size(src, srclen);
  return base64_decode_fast(dst, dstlen, src, srclen, decoded_size);
}

template size_t base64_decode<char>(char *const dst, const size_t dstlen,
                                    const char *const src, const size_t srclen);

} // namespace node