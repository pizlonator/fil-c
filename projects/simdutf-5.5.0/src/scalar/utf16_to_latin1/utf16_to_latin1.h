#ifndef SIMDUTF_UTF16_TO_LATIN1_H
#define SIMDUTF_UTF16_TO_LATIN1_H

namespace simdutf {
namespace scalar {
namespace {
namespace utf16_to_latin1 {

#include <cstring>  // for std::memcpy

template <endianness big_endian>
inline size_t convert(const char16_t* buf, size_t len, char* latin_output) {
  if(len == 0) { return 0; }
  const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  size_t pos = 0;
  std::vector<char> temp_output(len);
  char* current_write = temp_output.data();
  uint16_t word = 0;
  uint16_t too_large = 0;

  while (pos < len) {
    word = !match_system(big_endian) ? utf16::swap_bytes(data[pos]) : data[pos];
    too_large |= word;
    *current_write++ = char(word & 0xFF);
    pos++;
  }
  if((too_large & 0xFF00) != 0) { return 0; }

  // Only copy to latin_output if there were no errors
  std::memcpy(latin_output, temp_output.data(), len);

  return current_write - temp_output.data();
}

template <endianness big_endian>
inline result convert_with_errors(const char16_t* buf, size_t len, char* latin_output) {
  if(len == 0) { return result(error_code::SUCCESS,0); }
 const uint16_t *data = reinterpret_cast<const uint16_t *>(buf);
  size_t pos = 0;
  char* start{latin_output};
  uint16_t word;

  while (pos < len) {
    if (pos + 16 <= len) { // if it is safe to read 32 more bytes, check that they are Latin1
      uint64_t v1, v2, v3, v4;
      ::memcpy(&v1, data + pos, sizeof(uint64_t));
      ::memcpy(&v2, data + pos + 4, sizeof(uint64_t));
      ::memcpy(&v3, data + pos + 8, sizeof(uint64_t));
      ::memcpy(&v4, data + pos  + 12, sizeof(uint64_t));

      if (!match_system(big_endian)) { v1 = (v1 >> 8) | (v1 << (64 - 8)); }
      if (!match_system(big_endian)) { v2 = (v2 >> 8) | (v2 << (64 - 8)); }
      if (!match_system(big_endian)) { v3 = (v3 >> 8) | (v3 << (64 - 8)); }
      if (!match_system(big_endian)) { v4 = (v4 >> 8) | (v4 << (64 - 8)); }

      if (((v1 | v2 | v3 | v4) & 0xFF00FF00FF00FF00) == 0) {
        size_t final_pos = pos + 16;
        while(pos < final_pos) {
          *latin_output++ = !match_system(big_endian) ? char(utf16::swap_bytes(data[pos])) : char(data[pos]);
          pos++;
        }
        continue;
      }
    }
    word = !match_system(big_endian) ? utf16::swap_bytes(data[pos]) : data[pos];
    if((word & 0xFF00 ) == 0) {
        *latin_output++ = char(word & 0xFF);
        pos++;
    } else { return result(error_code::TOO_LARGE, pos); }
  }
  return result(error_code::SUCCESS,latin_output - start);
}


} // utf16_to_latin1 namespace
} // unnamed namespace
} // namespace scalar
} // namespace simdutf

#endif