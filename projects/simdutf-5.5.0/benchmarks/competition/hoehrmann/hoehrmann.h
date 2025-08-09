#ifndef HEOHRMANN_H
#define HEOHRMANN_H
#include <cstddef>
#include <cstdint>

namespace hoehrmann {
// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

constexpr int utf8_accept = 0;
constexpr int utf8_reject = 12;

static const uint8_t utf8d[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  9,  9,  9,  9,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  9,  9,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
    7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
    7,  7,  8,  8,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  10, 3,  3,  3,
    3,  3,  3,  3,  3,  3,  3,  3,  3,  4,  3,  3,  11, 6,  6,  6,  5,  8,  8,
    8,  8,  8,  8,  8,  8,  8,  8,  8,  0,  12, 24, 36, 60, 96, 84, 12, 12, 12,
    48, 72, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 0,  12, 12, 12,
    12, 12, 0,  12, 0,  12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 24, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12, 24, 12, 12, 12, 12, 12,
    12, 12, 24, 12, 12, 12, 12, 12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36,
    12, 12, 12, 12, 12, 36, 12, 36, 12, 12, 12, 36, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12,
};

static inline uint32_t decode(uint32_t *state, uint32_t *codep, uint32_t byte) {
  uint32_t type = utf8d[byte];

  *codep = (*state != utf8_accept) ? (byte & 0x3F) | (*codep << 6)
                                   : (0xFF >> type) & (byte);

  *state = utf8d[256 + *state + type];
  return *state;
}

static inline size_t toUtf32(uint8_t const *src, size_t srcBytes,
                                char32_t *dst) {
  uint8_t const *src_actual_end = src + srcBytes;
  uint8_t const *s = src;
  char32_t *d = dst;
  uint32_t state = 0;
  uint32_t codepoint = 0;

  while (s < src_actual_end) {
    if (decode(&state, &codepoint, *s++)) {
      continue;
    }
    *d++ = (char32_t)codepoint;
  }
  if(state == utf8_reject) { return 0; }
  return size_t(d - dst);
}

static inline size_t toUtf16(uint8_t const *src, size_t srcBytes,
                                char16_t *dst) {
  uint8_t const *src_actual_end = src + srcBytes;
  uint8_t const *s = src;
  char16_t *d = dst;
  uint32_t state = 0;
  uint32_t codepoint = 0;

  while (s < src_actual_end) {
    if (decode(&state, &codepoint, *s++)) {
      continue;
    }

    if (codepoint > 0xFFFF) {
      *d++ = (char16_t)(0xD7C0 + (codepoint >> 10));
      *d++ = (char16_t)(0xDC00 + (codepoint & 0x3FF));
    } else {
      *d++ = (char16_t)codepoint;
    }
  }
  if(state == utf8_reject) { return 0; }

  return size_t(d - dst);
}

} // namespace hoehrmann

#endif