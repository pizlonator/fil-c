#ifndef SIMDUTF_VALID_UTF32_TO_LATIN1_H
#define SIMDUTF_VALID_UTF32_TO_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf32_to_latin1 {

inline size_t convert_valid(const char32_t *buf, size_t len, char *latin1_output) {
  const uint32_t *data = reinterpret_cast<const uint32_t *>(buf);
  char* start = latin1_output;
  uint32_t utf32_char;
  size_t pos = 0;

  while (pos < len) {
      utf32_char = (uint32_t) data[pos];

      if (pos + 2 <= len) { // if it is safe to read 8 more bytes, check that they are Latin1
          uint64_t v;
          ::memcpy(&v, data + pos, sizeof(uint64_t));
          if ((v & 0xFFFFFF00FFFFFF00) == 0) {
              *latin1_output++ = char(buf[pos]);
              *latin1_output++ = char(buf[pos + 1]);
              pos += 2;
              continue;
          } else {
              // output can not be represented in latin1
              return 0;
          }
      }
      if ((utf32_char & 0xFFFFFF00) == 0) {
          *latin1_output++ = char(utf32_char);
      } else {
          // output can not be represented in latin1
          return 0;
      }
      pos++;
  }
  return latin1_output - start;
}

} // utf32_to_latin1 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif
