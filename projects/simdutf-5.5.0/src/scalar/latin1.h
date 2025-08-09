#ifndef SIMDUTF_LATIN1_H
#define SIMDUTF_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace latin1 {

inline size_t utf32_length_from_latin1(size_t len) {
  // We are not BOM aware.
  return len; // a utf32 unit will always represent 1 latin1 character
}

inline size_t utf8_length_from_latin1(const char *buf, size_t len) {
  const uint8_t * c = reinterpret_cast<const uint8_t *>(buf);
  size_t answer = 0;
  for(size_t i = 0; i<len; i++) {
    if((c[i]>>7)) { answer++; }
  }
  return answer + len;
}

inline size_t utf16_length_from_latin1(size_t len) {
  return len;
}

} // utf32 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif