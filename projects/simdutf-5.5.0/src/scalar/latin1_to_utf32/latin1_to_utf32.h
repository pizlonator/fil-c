#ifndef SIMDUTF_LATIN1_TO_UTF32_H
#define SIMDUTF_LATIN1_TO_UTF32_H

namespace simdutf {
namespace scalar {
namespace {
namespace latin1_to_utf32 {


inline size_t convert(const char *buf, size_t len, char32_t *utf32_output) {
  const unsigned char *data = reinterpret_cast<const unsigned char *>(buf);
  char32_t* start{utf32_output};
  for (size_t i = 0; i < len; i++) {
    *utf32_output++ = (char32_t)data[i];
  }
  return utf32_output - start;
}

} // latin1_to_utf32 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif